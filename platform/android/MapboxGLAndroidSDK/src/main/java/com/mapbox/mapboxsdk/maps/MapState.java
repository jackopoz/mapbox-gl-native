package com.mapbox.mapboxsdk.maps;

import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.UiThread;
import android.text.TextUtils;

import com.mapbox.mapboxsdk.constants.MapboxConstants;

class MapState extends State {

  private String apiBaseUrl;
  private String styleUrl;
  private boolean debug;

  MapState(@NonNull NativeMapView nativeMapView, @NonNull GLSurfaceView glSurfaceView) {
    super(nativeMapView, glSurfaceView);
  }

  @UiThread
  void setApiBaseUrl(@NonNull final String apiBaseUrl) {
    this.apiBaseUrl = apiBaseUrl;
    queueRenderEvent(new NativeMapRunnable() {
      @Override
      public void execute(NativeMapView nativeMapView) {
        nativeMapView.setApiBaseUrl(apiBaseUrl);
      }
    });
  }

  @UiThread
  String getStyleUrl() {
    return styleUrl;
  }

  @UiThread
  void setStyleUrl(final String styleUrl) {
    this.styleUrl = styleUrl;
    queueRenderEvent(new NativeMapRunnable() {
      @Override
      public void execute(NativeMapView nativeMapView) {
        nativeMapView.setStyleUrl(styleUrl);
      }
    });
  }

  @UiThread
  boolean getDebug() {
    return debug;
  }

  @UiThread
  void setDebug(final boolean debug) {
    this.debug = debug;
    queueRenderEvent(new NativeMapRunnable() {
      @Override
      public void execute(NativeMapView nativeMapView) {
        nativeMapView.setDebug(debug);
      }
    });
  }

  @UiThread
  void cycleDebugOptions(){
    queueRenderEvent(new NativeMapRunnable() {
      @Override
      public void execute(@NonNull NativeMapView nativeMapView) {
        nativeMapView.cycleDebugOptions();
      }
    });
  }

  @UiThread
  void onSaveInstanceState(Bundle outState) {
    outState.putBoolean(MapboxConstants.STATE_DEBUG_ACTIVE, debug);
    outState.putString(MapboxConstants.STATE_STYLE_URL, styleUrl);
    outState.putString(MapboxConstants.STATE_API_BASE_URL, apiBaseUrl);
  }

  @UiThread
  void onRestoreInstanceState(Bundle savedInstanceState) {
    boolean debug = savedInstanceState.getBoolean(MapboxConstants.STATE_DEBUG_ACTIVE);
    if(debug){
      setDebug(debug);
    }

    String styleUrl = savedInstanceState.getString(MapboxConstants.STATE_STYLE_URL);
    if (!TextUtils.isEmpty(styleUrl)) {
      setStyleUrl(styleUrl);
    }

    String apiBaseUrl = savedInstanceState.getString(MapboxConstants.STATE_API_BASE_URL);
    if (!TextUtils.isEmpty(apiBaseUrl)) {
      setApiBaseUrl(apiBaseUrl);
    }
  }
}
