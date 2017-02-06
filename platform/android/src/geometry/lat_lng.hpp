#pragma once

#include <mbgl/util/noncopyable.hpp>
#include <mbgl/util/geometry.hpp>

#include <jni/jni.hpp>

namespace mbgl {
namespace android {

class LatLng : private mbgl::util::noncopyable {
public:

    static constexpr auto Name() { return "com/mapbox/mapboxsdk/geometry/LatLng"; };

    static jni::Class<LatLng> javaClass;

    static mbgl::Point<double> getGeometry(jni::JNIEnv&, jni::Object<LatLng>);

    static void registerNative(jni::JNIEnv&);

};


} // namespace android
} // namespace mbgl