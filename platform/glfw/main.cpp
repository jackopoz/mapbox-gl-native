#include "glfw_view.hpp"
#include "settings_json.hpp"

#include <mbgl/util/default_styles.hpp>
#include <mbgl/util/logging.hpp>
#include <mbgl/util/platform.hpp>
#include <mbgl/util/default_thread_pool.hpp>
#include <mbgl/storage/default_file_source.hpp>

#include <signal.h>
#include <getopt.h>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cstdio>
#include <array>

namespace {

GLFWView* view = nullptr;

}

void quit_handler(int) {
    if (view) {
        mbgl::Log::Info(mbgl::Event::Setup, "waiting for quit...");
        view->setShouldClose();
    } else {
        exit(0);
    }
}

int main(int argc, char *argv[]) {
    bool fullscreen = false;
    bool benchmark = false;
    std::string style;
    double latitude = 0, longitude = 0;
    double bearing = 0, zoom = 1, pitch = 0;
    bool skipConfig = false;

    const struct option long_options[] = {
        {"fullscreen", no_argument, 0, 'f'},
        {"benchmark", no_argument, 0, 'b'},
        {"style", required_argument, 0, 's'},
        {"lon", required_argument, 0, 'x'},
        {"lat", required_argument, 0, 'y'},
        {"zoom", required_argument, 0, 'z'},
        {"bearing", required_argument, 0, 'r'},
        {"pitch", required_argument, 0, 'p'},
        {0, 0, 0, 0}
    };

    while (true) {
        int option_index = 0;
        int opt = getopt_long(argc, argv, "fbs:", long_options, &option_index);
        if (opt == -1) break;
        switch (opt)
        {
        case 0:
            if (long_options[option_index].flag != 0)
                break;
        case 'f':
            fullscreen = true;
            break;
        case 'b':
            benchmark = true;
            break;
        case 's':
            style = std::string("asset://") + std::string(optarg);
            break;
        case 'x':
            longitude = atof(optarg);
            skipConfig = true;
            break;
        case 'y':
            latitude = atof(optarg);
            skipConfig = true;
            break;
        case 'z':
            zoom = atof(optarg);
            skipConfig = true;
            break;
        case 'r':
            bearing = atof(optarg);
            skipConfig = true;
            break;
        case 'p':
            pitch = atof(optarg);
            skipConfig = true;
            break;
        default:
            break;
        }

    }

    // sigint handling
    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = quit_handler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);

    if (benchmark) {
        mbgl::Log::Info(mbgl::Event::General, "BENCHMARK MODE: Some optimizations are disabled.");
    }

    GLFWView backend(fullscreen, benchmark);
    view = &backend;

    mbgl::DefaultFileSource fileSource("/tmp/mbgl-cache.db", ".");

    // Set access token if present
    const char *token = getenv("MAPBOX_ACCESS_TOKEN");
    if (token == nullptr) {
        mbgl::Log::Warning(mbgl::Event::Setup, "no access token set. mapbox.com tiles won't work.");
    } else {
        fileSource.setAccessToken(std::string(token));
    }

    mbgl::ThreadPool threadPool(4);

    mbgl::Map map(backend, view->getSize(), view->getPixelRatio(), fileSource, threadPool);

    backend.setMap(&map);

    // Load settings
    mbgl::Settings_JSON settings;

    if (skipConfig) {
        map.setLatLngZoom(mbgl::LatLng(latitude, longitude), zoom);
        map.setBearing(bearing);
        map.setPitch(pitch);
        mbgl::Log::Info(mbgl::Event::General, "Location: %f/%f (z%.2f, %.2f deg)", latitude, longitude, zoom, bearing);
    } else {
        map.setLatLngZoom(mbgl::LatLng(settings.latitude, settings.longitude), settings.zoom);
        map.setBearing(settings.bearing);
        map.setPitch(settings.pitch);
        map.setDebug(mbgl::MapDebugOptions(settings.debug));
    }

    view->setChangeStyleCallback([&map] () {
        static uint8_t currentStyleIndex;

        if (++currentStyleIndex == mbgl::util::default_styles::numOrderedStyles) {
            currentStyleIndex = 0;
        }

        mbgl::util::default_styles::DefaultStyle newStyle = mbgl::util::default_styles::orderedStyles[currentStyleIndex];
        map.setStyleURL(newStyle.url);
        view->setWindowTitle(newStyle.name);

        mbgl::Log::Info(mbgl::Event::Setup, "Changed style to: %s", newStyle.name);
    });

    // Load style
    if (style.empty()) {
        const char *url = getenv("MAPBOX_STYLE_URL");
        if (url == nullptr) {
            mbgl::util::default_styles::DefaultStyle newStyle = mbgl::util::default_styles::orderedStyles[0];
            style = newStyle.url;
            view->setWindowTitle(newStyle.name);
        } else {
            style = url;
            view->setWindowTitle(url);
        }
    }

    map.setStyleURL(style);

    view->run();

    // Save settings
    mbgl::LatLng latLng = map.getLatLng();
    settings.latitude = latLng.latitude;
    settings.longitude = latLng.longitude;
    settings.zoom = map.getZoom();
    settings.bearing = map.getBearing();
    settings.pitch = map.getPitch();
    settings.debug = mbgl::EnumType(map.getDebug());
    if (!skipConfig) {
        settings.save();
    }
    mbgl::Log::Info(mbgl::Event::General,
                    "Exit location: --lat=\"%f\" --lon=\"%f\" --zoom=\"%f\" --bearing \"%f\"",
                    settings.latitude, settings.longitude, settings.zoom, settings.bearing);

    view = nullptr;
    return 0;
}
