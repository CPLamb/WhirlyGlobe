/*
 *  QuadDisplayLayer.h
 *  WhirlyGlobeLib
 *
 *  Created by Steve Gifford on 4/17/12.
 *  Copyright 2011-2013 mousebird consulting
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

#import <math.h>
#import "WhirlyVector.h"
#import "Scene.h"
#import "GlobeMath.h"
#import "Quadtree.h"
#import "SceneRendererES.h"
#import "ScreenImportance.h"
#import "ViewState.h"

namespace WhirlyKit
{
/// Quad tree Nodeinfo structures sorted by importance
typedef std::set<Quadtree::NodeInfo> QuadNodeInfoSet;
    
class QuadDisplayController;

/** Quad tree based data structure.  Fill this in to provide structure and
    extents for the quad tree.
 */
class QuadDataStructure
{
public:
    virtual ~QuadDataStructure() { }
    
    /// Return the coordinate system we're working in
    virtual CoordSystem *getCoordSystem() = 0;
    
    /// Bounding box used to calculate quad tree nodes.  In local coordinate system.
    virtual Mbr getTotalExtents() = 0;
    
    /// Bounding box of data you actually want to display.  In local coordinate system.
    /// Unless you're being clever, make this the same as totalExtents.
    virtual Mbr getValidExtents() = 0;
    
    /// Return the minimum quad tree zoom level (usually 0)
    virtual int getMinZoom() = 0;
    
    /// Return the maximum quad tree zoom level.  Must be at least minZoom
    virtual int getMaxZoom() = 0;
    
    /// Return an importance value for the given tile
    virtual double importanceForTile(const Quadtree::Identifier &ident,const Mbr &mbr,ViewState *viewState,const Point2f &frameSize,Dictionary *attrs) = 0;
    
    /// Called when the view state changes.  If you're caching info, do it here.
    virtual void newViewState(ViewState *viewState) = 0;

    /// Called when the layer is shutting down.  Clean up any drawable data and clear out caches.
    virtual void shutdown() = 0;
};


/** Loader protocol for quad tree changes.  Fill this in to be
    notified when the quad layer is adding and removing tiles.
    Presumably you'll want to add or remove geometry as well.
 */
class QuadLoader
{
public:
    QuadLoader() : control(NULL), scene(NULL) { }
    virtual ~QuadLoader() { }
    
    /// Called when the layer first starts up.  Keep this around if you need it.
    virtual void init(QuadDisplayController *inControl,Scene *inScene) { control = inControl; scene = inScene;}

    /// The quad layer uses this to see if a loader is capable of loading
    ///  another tile.  Use this to track simultaneous loads
    virtual bool isReady() = 0;

    /// Called right before we start a series of updates
    virtual void startUpdates() = 0;

    /// Called right after we finish a series of updates
    virtual void endUpdates() = 0;

    /// The quad tree wants to load the given tile.
    /// Call the layer back when the tile is loaded.
    /// This is in the layer thread.
    virtual void loadTile(const Quadtree::NodeInfo &tileInfo) = 0;

    /// Quad tree wants to unload the given tile immediately.
    /// This is in the layer thread.
    virtual void unloadTile(const Quadtree::NodeInfo &tileInfo) = 0;

    /// The layer is checking to see if it's allowed to traverse below the given tile.
    /// If the loader is still trying to load that given tile (or has some other information about it),
    ///  then return false.  If the tile is loaded and the children may be valid, return true.
    virtual bool canLoadChildrenOfTile(const Quadtree::NodeInfo &tileInfo) = 0;

    /// Called when the layer is about to shut down.  Clear out any drawables and caches.
    virtual void shutdownLayer() = 0;

    /// Called right before the view update to determine if we should even be paging
    /// You can use this to temporarily suspend paging.
    /// isInitial is set if this is the first time through
    virtual bool shouldUpdate(ViewState *viewState,bool isInitial) = 0;

    /// Normally we'd call an endUpdates, but if we're holding that open for a while
    /// (e.g. matching frame boundaries), let's at least get all the work done.
    virtual void updateWithoutFlush() { };

    /// Number of network fetches outstanding.  Used by the pager for optimization.
    virtual int networkFetches() { return -1; };

    /// Number of local fetches outstanding.  Used by the pager for optimizaiton.
    virtual int localFetches() { return -1; };

    /// Dump some log info out to the console
    virtual void log() { };
    
protected:
    QuadDisplayController *control;
    Scene *scene;
};

/** This is a base class that versions of the toolkit fill in for specific platforms.
    For Obj-C we do one thing, for Android another, etc..
    The QuadDisplayController calls back into this object when various things need to happen.
  */
class QuadDisplayControllerAdapter
{
public:
    virtual ~QuadDisplayControllerAdapter() { }
    // Called right after a tile loaded
    virtual void tileDidLoad(const Quadtree::Identifier &tileIdent) = 0;
    // Called right after a tile unloaded
    virtual void tileDidNotLoad(const Quadtree::Identifier &tileIdent) = 0;
};

/** This data layer displays image data organized in a quad tree.
    It will swap data in and out as required.
 */
class QuadDisplayController : public QuadTreeImportanceCalculator
{
public:
    QuadDisplayController(QuadDataStructure *dataStructure,QuadLoader *loader,QuadDisplayControllerAdapter *adapter);
    ~QuadDisplayController();

    // Called when we ready to start doing things
    void init(Scene *scene,SceneRendererES *renderer);
    
    /// Data source for the quad tree structure
    QuadDataStructure *getDataStructure() { return dataStructure; }
    /// Loader that may be creating and deleting data as the quad tiles load
    ///  and unload.
    QuadLoader *getLoader() { return loader; }
    /// The quad tree that's doing the spatial reasoning
    Quadtree *getQuadtree() { return quadtree; }

    /// Scene we're modifying
    Scene *getScene() { return scene; }
    /// Renderer we're attached to
    SceneRendererES *getRenderer() { return renderer; }
    
    /// Coordinate system we're working in for tiling
    CoordSystem *getCoordSys() { return coordSys; }
    /// Bounding box we're paging over
    const Mbr &getMbr() { return mbr; }
    
    /// Minimum screen area to consider for a pixel
    float getMinImportance() const { return minImportance; }
    void setMinImportance(float newMinImport) { minImportance = newMinImport; if (quadtree) quadtree->setMinImportance(newMinImport); }
    /// Maximum number of tiles loaded in at once
    int getMaxTiles() const { return maxTiles; }
    void setMaxTiles(int newMaxTiles) { maxTiles = newMaxTiles; if (quadtree) quadtree->setMaxNodes(maxTiles); }
    /// [minZoom,maxZoom] range
    void setZoom(int inMinZoom,int inMaxZoom) { minZoom = inMinZoom;  maxZoom = inMaxZoom; }
    
    /// If set the eval step gets very aggressive about loading tiles.
    /// This will slow down the layer thread, but makes the quad layer appear faster
    bool getGreedyMode() { return greedyMode; }
    void setGreedyMode(bool inGreedyMode) { greedyMode = inGreedyMode; }
    /// Metered mode tracks frame update boundaries to sync updates
    bool getMeteredMode() { return meteredMode; }
    void setMeteredMode(bool newMeteredMode) { meteredMode = newMeteredMode; }
    /// Set if we're supposed to be waiting for local loads (e.g. a reload)
    bool getWaitForLocalLoads() { return waitForLocalLoads; }
    void setWaitForLocalLoads(bool newMode) { waitForLocalLoads = newMode; }
    /// If fullLoad is on we'll try to wait until everything is loaded before displaying
    bool getFullLoad() { return fullLoad; }
    void setFullLoad(bool newVal) { fullLoad = newVal; }
    /// If fullLoad is on, we need a timeout.  Otherwise changes just pile up until we run out of memory
    TimeInterval getFullLoadTimeout() { return fullLoadTimeout; }
    void setFullLoadTimeout(TimeInterval newTimeout) { fullLoadTimeout = newTimeout; }
    /// How often this layer gets notified of view changes.  1s by default.
    TimeInterval getViewUpdatePeriod() { return viewUpdatePeriod; }
    void setViewUpdatePeriod(TimeInterval newPeriod) { viewUpdatePeriod = newPeriod; }
    /// How far the viewer has to move to force an update (if non-zero)
    float getMinUpdateDist() { return minUpdateDist; }
    void setMinUpdateDist(float newDist) { minUpdateDist = newDist; }

    /// Draw lines instead of polygons, for demonstration.
    bool getLineMode() { return lineMode; }
    void setLineMode(bool newLineMode) { lineMode = newLineMode; }
    /// If set, we print out way too much debugging info.
    bool getDebugMode() { return debugMode; }
    void setDebugMode(bool newDebugMode) { debugMode = newDebugMode; }

    /// When we last flushed in metered mode
    void setLastFlush(TimeInterval when) { lastFlush = when; }
    TimeInterval getLastFlush() { return lastFlush; }
    
    /// Something happened with recent updates.  This means we need to flush at some point.
    bool getSomethingHappened() { return somethingHappened; }
    
    /// Set if we haven't gotten an update yet
    bool getFirstUpdate() { return firstUpdate; }

    /// A loader calls this after successfully loading a tile.
    void tileDidLoad(const Quadtree::Identifier &tileIdent);

    /// Loader calls this after a failed tile load.
    void tileDidNotLoad(const Quadtree::Identifier &tileIdent);
    
    // Called every so often by the view watcher
    // It's here that we evaluate what to load
    void viewUpdate(ViewState *inViewState);
    
    // Called at regular intervals to do a small bit of work, then returns.
    // Returns true if there's more work to do
    bool evalStep(TimeInterval frameStart,TimeInterval frameInterval,float availableFrame);
    
    // Called near the end of a frame in metered mode
    void frameEnd();

    // True if we're waiting for local loads to finish (looks faster to the user)
    bool waitingForLocalLoads();
    
    // Called when the layer wants to shut down
    void shutdown();

    /// Call this to force a reload for all existing tiles
    void refresh();

    // If we were waiting for something, apparently we no longer are
    void wakeUp();
    
    // Callback used by the quad tree
    virtual double importanceForTile(const Quadtree::Identifier &ident,const Mbr &theMbr,Quadtree *tree,Dictionary *attrs);

    // Debugging output
    void dumpInfo();
    
protected:
    QuadDisplayControllerAdapter *adapter;
    QuadDataStructure *dataStructure;
    QuadLoader *loader;
    Quadtree *quadtree;

    Scene *scene;
    SceneRendererES *renderer;
    
    CoordSystem *coordSys;
    Mbr mbr;
    
    float minImportance;
    int maxTiles;
    int minZoom,maxZoom;
    
    bool greedyMode;
    bool meteredMode;
    bool waitForLocalLoads;
    bool fullLoad;
    TimeInterval fullLoadTimeout;
    TimeInterval viewUpdatePeriod;
    float minUpdateDist;

    bool lineMode;
    bool debugMode;

    /// Nodes being evaluated for loading
    QuadNodeInfoSet nodesForEval;
    
    /// State of the view the last time we were called
    ViewState viewState;
    
    // In metered mode, the last time we flushed data to the scene
    TimeInterval lastFlush;
    
    // In metered mode, we'll only flush if something happened
    bool somethingHappened;

    bool firstUpdate;
};
    
}

