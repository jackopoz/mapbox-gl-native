package com.mapbox.mapboxsdk.maps;

import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.UiThread;
import android.support.annotation.WorkerThread;
import android.text.TextUtils;

import com.mapbox.mapboxsdk.constants.MapboxConstants;

class MapState {

  private String apiBaseUrl;
  private String styleUrl;
  private boolean debug;
  private int cycleDebugOptions;

  private boolean baseUrlNeedsSync;
  private boolean styleUrlNeedsSync;
  private boolean debugNeedsSync;
  private boolean cycleDebugNeedsSync;

  @UiThread
  void setApiBaseUrl(String apiBaseUrl) {
    this.apiBaseUrl = apiBaseUrl;
    baseUrlNeedsSync = true;
  }

  @UiThread
  String getStyleUrl() {
    return styleUrl;
  }

  @UiThread
  void setStyleUrl(String styleUrl) {
    this.styleUrl = styleUrl;
    styleUrlNeedsSync = true;
  }

  @UiThread
  void onSaveInstanceState(Bundle outState) {
    outState.putBoolean(MapboxConstants.STATE_DEBUG_ACTIVE, debug);
    outState.putString(MapboxConstants.STATE_STYLE_URL, styleUrl);
    outState.putString(MapboxConstants.STATE_API_BASE_URL, apiBaseUrl);
  }

  @UiThread
  void onRestoreInstanceState(Bundle savedInstanceState) {
    debug = savedInstanceState.getBoolean(MapboxConstants.STATE_DEBUG_ACTIVE);
    if(debug){
      debugNeedsSync = true;
    }

    final String styleUrl = savedInstanceState.getString(MapboxConstants.STATE_STYLE_URL);
    if (!TextUtils.isEmpty(styleUrl)) {
      setStyleUrl(styleUrl);
    }

    final String apiBaseUrl = savedInstanceState.getString(MapboxConstants.STATE_API_BASE_URL);
    if (!TextUtils.isEmpty(apiBaseUrl)) {
      setApiBaseUrl(apiBaseUrl);
    }
  }

  @UiThread
  boolean getDebug() {
    return debug;
  }

  @UiThread
  void cycleDebugOptions() {
    cycleDebugOptions++;
  }

  @UiThread
  void setDebug(boolean debug) {
    this.debug = debug;
  }

  @WorkerThread
  void syncState(@NonNull NativeMapView nativeMapView) {
    if (baseUrlNeedsSync) {
      nativeMapView.setApiBaseUrl(apiBaseUrl);
      baseUrlNeedsSync = false;
    } else if (styleUrlNeedsSync) {
      nativeMapView.setStyleUrl(styleUrl);
      styleUrlNeedsSync = false;
    } else if (debugNeedsSync) {
      nativeMapView.setDebug(debug);
      debugNeedsSync = false;
    } else if (cycleDebugNeedsSync) {
      for (int i = 0; i < cycleDebugOptions; i++) {
        nativeMapView.cycleDebugOptions();
      }
      cycleDebugNeedsSync = false;
      cycleDebugOptions = 0;
    }
  }
}
