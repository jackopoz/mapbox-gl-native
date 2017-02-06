#include "lat_lng.hpp"

namespace mbgl {
namespace android {

mbgl::Point<double> LatLng::getGeometry(jni::JNIEnv& env, jni::Object<LatLng> marker) {
    static auto latitudeField = LatLng::javaClass.GetField<jni::jdouble>(env, "latitude");
    static auto longitudeField = LatLng::javaClass.GetField<jni::jdouble>(env, "longitude");
    return mbgl::Point<double>(marker.Get(env, longitudeField), marker.Get(env, latitudeField));
}

void LatLng::registerNative(jni::JNIEnv& env) {
    // Lookup the class
    LatLng::javaClass = *jni::Class<LatLng>::Find(env).NewGlobalRef(env).release();
}

jni::Class<LatLng> LatLng::javaClass;


} // namespace android
} // namespace mbgl