#include "native_map_view.hpp"
#include "jni.hpp"
#include <jni/jni.hpp>
#include "attach_env.hpp"

#include <cstdlib>
#include <ctime>
#include <cassert>
#include <memory>
#include <list>
#include <tuple>

#include <sys/system_properties.h>

#include <EGL/egl.h>

#include <mbgl/util/platform.hpp>
#include <mbgl/util/event.hpp>
#include <mbgl/util/logging.hpp>
#include <mbgl/gl/extension.hpp>
#include <mbgl/gl/context.hpp>
#include <mbgl/util/constants.hpp>
#include <mbgl/util/image.hpp>

#include "bitmap.hpp"
#include "run_loop_impl.hpp"

namespace mbgl {
namespace android {

using DebugOptions = mbgl::MapDebugOptions;

NativeMapView::NativeMapView(jni::JNIEnv& _env, jni::Object<NativeMapView> _obj, jni::String _cachePath, jni::String _apkPath,
                             jni::jfloat _pixelRatio, jni::jint _availableProcessors, jni::jlong _totalMemory) :
    javaPeer(_obj.NewWeakGlobalRef(_env)),
    pixelRatio(_pixelRatio),
    availableProcessors(_availableProcessors),
    totalMemory(_totalMemory),
    runLoop(std::make_unique<mbgl::util::RunLoop>(mbgl::util::RunLoop::Type::New)),
    threadPool(4) {

    mbgl::Log::Info(mbgl::Event::Android, "NativeMapView::NativeMapView");

    //Add a wake function to wake the render thread when needed
    mbgl::util::RunLoop::Impl* loop = reinterpret_cast<mbgl::util::RunLoop::Impl*>(mbgl::util::RunLoop::getLoopHandle());
    loop->setWakeFunction(std::bind(&NativeMapView::wake, this));

    // Get a reference to the JavaVM for callbacks
    //TODO: Why?
    if (_env.GetJavaVM(&vm) < 0) {
        _env.ExceptionDescribe();
        return;
    }

    // Create a default file source for this map instance
    fileSource = std::make_unique<mbgl::DefaultFileSource>(
        jni::Make<std::string>(_env, _cachePath) + "/mbgl-offline.db",
        jni::Make<std::string>(_env, _apkPath));

    // Create the core map
    map = std::make_unique<mbgl::Map>(
        *this, mbgl::Size{ static_cast<uint32_t>(width), static_cast<uint32_t>(height) },
        pixelRatio, *fileSource, threadPool, MapMode::Continuous);

    //Calculate a fitting cache size based on device parameters
    float zoomFactor   = map->getMaxZoom() - map->getMinZoom() + 1;
    float cpuFactor    = availableProcessors;
    float memoryFactor = static_cast<float>(totalMemory) / 1000.0f / 1000.0f / 1000.0f;
    float sizeFactor   = (static_cast<float>(map->getSize().width)  / mbgl::util::tileSize) *
        (static_cast<float>(map->getSize().height) / mbgl::util::tileSize);

    map->setSourceTileCacheSize(zoomFactor * cpuFactor * memoryFactor * sizeFactor * 0.5f);
}

/**
 * Called from Finalizer thread, not the rendering thread.
 * Don't mess with the state here
 */
NativeMapView::~NativeMapView() {
    mbgl::Log::Info(mbgl::Event::Android, "NativeMapView::~NativeMapView");
}

/**
 * From mbgl::View
 */
void NativeMapView::bind() {
    getContext().bindFramebuffer = 0;
    getContext().viewport = { 0, 0, getFramebufferSize() };
}

/**
 * From mbgl::Backend. Callback to java NativeMapView#onInvalidate().
 *
 * May be called from any thread
 */
void NativeMapView::invalidate() {
    Log::Info(mbgl::Event::Android, "NativeMapView::invalidate");
    android::UniqueEnv _env = android::AttachEnv();
    static auto onInvalidate = javaClass.GetMethod<void ()>(*_env, "onInvalidate");
    javaPeer->Call(*_env, onInvalidate);
}

/**
 * From mbgl::Backend. Callback to java NativeMapView#onMapChanged(int).
 *
 * May be called from any thread
 */
void NativeMapView::notifyMapChange(mbgl::MapChange change) {
    mbgl::Log::Info(mbgl::Event::Android, "notifyMapChange");
    assert(vm != nullptr);

    android::UniqueEnv _env = android::AttachEnv();
    static auto onMapChanged = javaClass.GetMethod<void (int)>(*_env, "onMapChanged");
    javaPeer->Call(*_env, onMapChanged, (int) change);
}

// JNI Methods //

/**
 * Custom destroy function for cleanup that needs to be run on the
 * render thread.
 */
void NativeMapView::destroy(jni::JNIEnv&) {
    mbgl::Log::Info(mbgl::Event::Android, "NativeMapView::destroy");

    //Remove the wake function as the JVM object is going to be GC'd pretty soon
    mbgl::util::RunLoop::Impl* loop = reinterpret_cast<mbgl::util::RunLoop::Impl*>(mbgl::util::RunLoop::getLoopHandle());
    loop->setWakeFunction(nullptr);

    map.reset();
    fileSource.reset();

    vm = nullptr;
}

void NativeMapView::onViewportChanged(jni::JNIEnv&, jni::jint w, jni::jint h) {
    resizeView((int) w / pixelRatio, (int) h / pixelRatio);
    resizeFramebuffer(w, h);
}

void NativeMapView::render(jni::JNIEnv& env) {
    mbgl::Log::Info(mbgl::Event::Android, "NativeMapView::render");

    if (firstRender) {
        mbgl::Log::Info(mbgl::Event::Android, "Initialize GL extensions");
        mbgl::gl::InitializeExtensions([] (const char * name) {
             return reinterpret_cast<mbgl::gl::glProc>(eglGetProcAddress(name));
        });
        firstRender = false;
    }

    //First, spin the run loop to process the queue (as there is no actual loop on the render thread)
    mbgl::util::RunLoop::Get()->runOnce();

    if (framebufferSizeChanged) {
        getContext().viewport = { 0, 0, getFramebufferSize() };
        framebufferSizeChanged = false;
    }

    updateViewBinding();
    map->render(*this);

    if (snapshot){
        snapshot = false;

        // take snapshot
        auto image = getContext().readFramebuffer<mbgl::PremultipliedImage>(getFramebufferSize());
        auto bitmap = Bitmap::CreateBitmap(env, std::move(image));

        android::UniqueEnv _env = android::AttachEnv();
        static auto onSnapshotReady = javaClass.GetMethod<void (jni::Object<Bitmap>)>(*_env, "onSnapshotReady");
        javaPeer->Call(*_env, onSnapshotReady, bitmap);
    }

    updateFps();
}

void NativeMapView::setAPIBaseUrl(jni::JNIEnv& env, jni::String url) {
    fileSource->setAPIBaseURL(jni::Make<std::string>(env, url));
}

jni::String NativeMapView::getStyleUrl(jni::JNIEnv& env) {
    return jni::Make<jni::String>(env, map->getStyleURL());
}

void NativeMapView::setStyleUrl(jni::JNIEnv& env, jni::String url) {
    map->setStyleURL(jni::Make<std::string>(env, url));
}

jni::String NativeMapView::getStyleJson(jni::JNIEnv& env) {
    return jni::Make<jni::String>(env, map->getStyleJSON());
}

void NativeMapView::setStyleJson(jni::JNIEnv& env, jni::String json) {
    map->setStyleJSON(jni::Make<std::string>(env, json));
}

jni::String NativeMapView::getAccessToken(jni::JNIEnv& env) {
    return jni::Make<jni::String>(env, fileSource->getAccessToken());
}

void NativeMapView::setAccessToken(jni::JNIEnv& env, jni::String token) {
    fileSource->setAccessToken(jni::Make<std::string>(env, token));
}

void NativeMapView::cancelTransitions(jni::JNIEnv&) {
    map->cancelTransitions();
}

void NativeMapView::setGestureInProgress(jni::JNIEnv&, jni::jboolean inProgress) {
    map->setGestureInProgress(inProgress);
}

void NativeMapView::moveBy(jni::JNIEnv&, jni::jdouble dx, jni::jdouble dy) {
    map->moveBy({dx, dy});
}

void NativeMapView::setLatLng(jni::JNIEnv&, jni::jdouble latitude, jni::jdouble longitude) {
    map->setLatLng(mbgl::LatLng(latitude, longitude), insets);
}

void NativeMapView::setReachability(jni::JNIEnv&, jni::jboolean reachable) {
    if (reachable) {
        mbgl::NetworkStatus::Reachable();
    }
}

void NativeMapView::resetPosition(jni::JNIEnv&) {
    map->resetPosition();
}

jni::jdouble NativeMapView::getPitch(jni::JNIEnv&) {
    return map->getPitch();
}

void NativeMapView::setPitch(jni::JNIEnv&, jni::jdouble pitch) {
    map->setPitch(pitch);
}

void NativeMapView::scaleBy(jni::JNIEnv&, jni::jdouble ds, jni::jdouble cx, jni::jdouble cy) {
    mbgl::ScreenCoordinate center(cx, cy);
    map->scaleBy(ds, center);
}

void NativeMapView::setScale(jni::JNIEnv&, jni::jdouble scale, jni::jdouble cx, jni::jdouble cy) {
    mbgl::ScreenCoordinate center(cx, cy);
    map->setScale(scale, center);
}

jni::jdouble NativeMapView::getScale(jni::JNIEnv&) {
    return map->getScale();
}

void NativeMapView::setZoom(jni::JNIEnv&, jni::jdouble zoom) {
    map->setZoom(zoom);
}

jni::jdouble NativeMapView::getZoom(jni::JNIEnv&) {
    return map->getZoom();
}

void NativeMapView::resetZoom(jni::JNIEnv&) {
    map->resetZoom();
}

void NativeMapView::setMinZoom(jni::JNIEnv&, jni::jdouble zoom) {
    map->setMinZoom(zoom);
}

jni::jdouble NativeMapView::getMinZoom(jni::JNIEnv&) {
    return map->getMinZoom();
}

void NativeMapView::setMaxZoom(jni::JNIEnv&, jni::jdouble zoom) {
    map->setMaxZoom(zoom);
}

jni::jdouble NativeMapView::getMaxZoom(jni::JNIEnv&) {
    return map->getMaxZoom();
}

void NativeMapView::rotateBy(jni::JNIEnv&, jni::jdouble sx, jni::jdouble sy, jni::jdouble ex, jni::jdouble ey) {
    mbgl::ScreenCoordinate first(sx, sy);
    mbgl::ScreenCoordinate second(ex, ey);
    map->rotateBy(first, second);
}

void NativeMapView::setBearing(jni::JNIEnv&, jni::jdouble degrees) {
    map->setBearing(degrees);
}

void NativeMapView::setBearingXY(jni::JNIEnv&, jni::jdouble degrees, jni::jdouble cx, jni::jdouble cy) {
    mbgl::ScreenCoordinate center(cx, cy);
    map->setBearing(degrees, center);
}

jni::jdouble NativeMapView::getBearing(jni::JNIEnv&) {
    return map->getBearing();
}

void NativeMapView::resetNorth(jni::JNIEnv&) {
    map->resetNorth();
}

void NativeMapView::scheduleSnapshot(jni::JNIEnv&) {
    snapshot = true;
}

void NativeMapView::enableFps(jni::JNIEnv&, jni::jboolean enable) {
    fpsEnabled = enable;
}

jni::Array<jni::jdouble> NativeMapView::getCameraValues(jni::JNIEnv& env) {
    //Create buffer with values
    jdouble buf[5];
    mbgl::LatLng latLng = map->getLatLng(insets);
    buf[0] = latLng.latitude;
    buf[1] = latLng.longitude;
    buf[2] = -map->getBearing();
    buf[3] = map->getPitch();
    buf[4] = map->getZoom();

    //Convert to Java array
    auto output = jni::Array<jni::jdouble>::New(env, 5);
    jni::SetArrayRegion(env, *output, 0, 5, buf);

    return output;
}

void NativeMapView::updateMarker(jni::JNIEnv& env, jni::jlong markerId, jni::jdouble lat, jni::jdouble lon, jni::String jid) {
    if (markerId == -1) {
        return;
    }

    std::string iconId = jni::Make<std::string>(env, jid);
    // Because Java only has int, not unsigned int, we need to bump the annotation id up to a long.
    map->updateAnnotation(markerId, mbgl::SymbolAnnotation { mbgl::Point<double>(lon, lat), iconId });
}

jni::Array<jni::jlong> NativeMapView::addMarkers(jni::JNIEnv& env, jni::Array<jni::Object<Marker>> jmarkers) {
    jni::NullCheck(env, &jmarkers);
    std::size_t len = jmarkers.Length(env);

    std::vector<jni::jlong> ids;
    ids.reserve(len);

    for (std::size_t i = 0; i < len; i++) {
        jni::Object<Marker> marker = jmarkers.Get(env, i);
        ids.push_back(map->addAnnotation(mbgl::SymbolAnnotation {
            Marker::getPosition(env, marker),
            Marker::getIconId(env, marker)
        }));

        jni::DeleteLocalRef(env, marker);
    }

    auto result = jni::Array<jni::jlong>::New(env, len);
    result.SetRegion<std::vector<jni::jlong>>(env, 0, ids);

    return result;
}

void NativeMapView::onLowMemory(JNIEnv&) {
    map->onLowMemory();
}

void NativeMapView::setDebug(JNIEnv&, jni::jboolean debug) {
    DebugOptions debugOptions = debug ? DebugOptions::TileBorders | DebugOptions::ParseStatus | DebugOptions::Collision
                                      : DebugOptions::NoDebug;
    map->setDebug(debugOptions);
    fpsEnabled = debug;
}

void NativeMapView::cycleDebugOptions(JNIEnv&) {
    map->cycleDebugOptions();
    fpsEnabled = map->getDebug() != DebugOptions::NoDebug;
}

jni::jboolean NativeMapView::getDebug(JNIEnv&) {
    return map->getDebug() != DebugOptions::NoDebug;
}

jni::jboolean NativeMapView::isFullyLoaded(JNIEnv&) {
    return map->isFullyLoaded();
}

jni::jdouble NativeMapView::getMetersPerPixelAtLatitude(JNIEnv&, jni::jdouble lat, jni::jdouble zoom) {
    return map->getMetersPerPixelAtLatitude(lat, zoom);
}

jni::Object<ProjectedMeters> NativeMapView::projectedMetersForLatLng(JNIEnv& env, jni::jdouble latitude, jni::jdouble longitude) {
    mbgl::ProjectedMeters projectedMeters = map->projectedMetersForLatLng(mbgl::LatLng(latitude, longitude));
    return ProjectedMeters::New(env, projectedMeters.northing, projectedMeters.easting);
}


// Private methods //

mbgl::Size NativeMapView::getFramebufferSize() const {
    mbgl::Log::Info(mbgl::Event::Android, "FB size %dx%d", fbWidth, fbHeight);
    return { static_cast<uint32_t>(fbWidth), static_cast<uint32_t>(fbHeight) };
}

/**
 * Called whenever the associated thread needs to wake up (since there is no active run loop)
 *
 * May be called from any thread
 */
void NativeMapView::wake() {
    mbgl::Log::Info(mbgl::Event::JNI, "Wake callback");
    android::UniqueEnv _env = android::AttachEnv();
    static auto wakeCallback = javaClass.GetMethod<void ()>(*_env, "onWake");
    javaPeer->Call(*_env, wakeCallback);
}

void NativeMapView::updateViewBinding() {
    getContext().bindFramebuffer.setCurrentValue(0);
    assert(mbgl::gl::value::BindFramebuffer::Get() == getContext().bindFramebuffer.getCurrentValue());
    getContext().viewport.setCurrentValue({ 0, 0, getFramebufferSize() });
    assert(mbgl::gl::value::Viewport::Get() == getContext().viewport.getCurrentValue());
}

void NativeMapView::resizeView(int w, int h) {
    mbgl::Log::Info(mbgl::Event::Android, "resizeView %ix%i", w, h);
    width = w;
    height = h;
    map->setSize({ static_cast<uint32_t>(width), static_cast<uint32_t>(height) });
}

void NativeMapView::resizeFramebuffer(int w, int h) {
    mbgl::Log::Info(mbgl::Event::Android, "resizeFramebuffer %ix%i", w, h);
    fbWidth = w;
    fbHeight = h;
    framebufferSizeChanged = true;
    invalidate();
}

void NativeMapView::updateFps() {
    if (!fpsEnabled) {
        return;
    }

    static int frames = 0;
    static int64_t timeElapsed = 0LL;

    frames++;
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    int64_t currentTime = now.tv_sec * 1000000000LL + now.tv_nsec;

    if (currentTime - timeElapsed >= 1) {
        fps = frames / ((currentTime - timeElapsed) / 1E9);
        mbgl::Log::Info(mbgl::Event::Render, "FPS: %4.2f", fps);
        timeElapsed = currentTime;
        frames = 0;
    }

    assert(vm != nullptr);

    android::UniqueEnv _env = android::AttachEnv();
    static auto onFpsChanged = javaClass.GetMethod<void (double)>(*_env, "onFpsChanged");
    javaPeer->Call(*_env, onFpsChanged, fps);
}

// Static methods //

jni::Class<NativeMapView> NativeMapView::javaClass;

void NativeMapView::registerNative(jni::JNIEnv& env) {
    // Lookup the class
    NativeMapView::javaClass = *jni::Class<NativeMapView>::Find(env).NewGlobalRef(env).release();

    #define METHOD(MethodPtr, name) jni::MakeNativePeerMethod<decltype(MethodPtr), (MethodPtr)>(name)

    // Register the peer
    jni::RegisterNativePeer<NativeMapView>(env, NativeMapView::javaClass, "nativePtr",
            std::make_unique<NativeMapView, JNIEnv&, jni::Object<NativeMapView>, jni::String, jni::String, jni::jfloat, jni::jint, jni::jlong>,
            "initialize",
            "finalize",
            METHOD(&NativeMapView::destroy, "destroy"),
            METHOD(&NativeMapView::render, "render"),
            METHOD(&NativeMapView::onViewportChanged, "_onViewportChanged"),
            METHOD(&NativeMapView::setAPIBaseUrl  , "setApiBaseUrl"),
            METHOD(&NativeMapView::getStyleUrl, "getStyleUrl"),
            METHOD(&NativeMapView::setStyleUrl, "setStyleUrl"),
            METHOD(&NativeMapView::getStyleJson, "getStyleJson"),
            METHOD(&NativeMapView::setStyleJson, "setStyleJson"),
            METHOD(&NativeMapView::getAccessToken, "getAccessToken"),
            METHOD(&NativeMapView::setAccessToken, "setAccessToken"),
            METHOD(&NativeMapView::cancelTransitions, "cancelTransitions"),
            METHOD(&NativeMapView::setGestureInProgress, "setGestureInProgress"),
            METHOD(&NativeMapView::moveBy, "_moveBy"),
            METHOD(&NativeMapView::setLatLng, "setLatLng"),
            METHOD(&NativeMapView::setReachability, "setReachability"),
            METHOD(&NativeMapView::resetPosition, "resetPosition"),
            METHOD(&NativeMapView::getPitch, "getPitch"),
            METHOD(&NativeMapView::setPitch, "setPitch"),
            METHOD(&NativeMapView::scaleBy, "_scaleBy"),
            METHOD(&NativeMapView::setScale, "_setScale"),
            METHOD(&NativeMapView::getScale, "getScale"),
            METHOD(&NativeMapView::setZoom, "setZoom"),
            METHOD(&NativeMapView::getZoom, "getZoom"),
            METHOD(&NativeMapView::resetZoom, "resetZoom"),
            METHOD(&NativeMapView::setMinZoom, "setMinZoom"),
            METHOD(&NativeMapView::getMinZoom, "getMinZoom"),
            METHOD(&NativeMapView::setMaxZoom, "setMaxZoom"),
            METHOD(&NativeMapView::getMaxZoom, "getMaxZoom"),
            METHOD(&NativeMapView::rotateBy, "_rotateBy"),
            METHOD(&NativeMapView::setBearing, "setBearing"),
            METHOD(&NativeMapView::setBearingXY, "_setBearingXY"),
            METHOD(&NativeMapView::getBearing, "getBearing"),
            METHOD(&NativeMapView::resetNorth, "resetNorth"),
            METHOD(&NativeMapView::scheduleSnapshot, "scheduleSnapshot"),
            METHOD(&NativeMapView::enableFps, "enableFps"),
            METHOD(&NativeMapView::getCameraValues, "getCameraValues"),
            METHOD(&NativeMapView::updateMarker, "_updateMarker"),
            METHOD(&NativeMapView::addMarkers, "addMarkers"),
            METHOD(&NativeMapView::setDebug, "setDebug"),
            METHOD(&NativeMapView::cycleDebugOptions, "cycleDebugOptions"),
            METHOD(&NativeMapView::getDebug, "getDebug"),
            METHOD(&NativeMapView::isFullyLoaded, "isFullyLoaded"),
            METHOD(&NativeMapView::onLowMemory, "onLowMemory"),
            METHOD(&NativeMapView::getMetersPerPixelAtLatitude, "getMetersPerPixelAtLatitude"),
            METHOD(&NativeMapView::projectedMetersForLatLng, "projectedMetersForLatLng")
    );
}

}
}
