package com.mapbox.mapboxsdk.maps;

import android.opengl.GLSurfaceView;
import android.support.annotation.NonNull;
import android.support.annotation.WorkerThread;

public abstract class State {

  private NativeMapView nativeMapView;
  private GLSurfaceView glSurfaceView;

  State(@NonNull NativeMapView nativeMapView, @NonNull GLSurfaceView glSurfaceView) {
    this.nativeMapView = nativeMapView;
    this.glSurfaceView = glSurfaceView;
  }

  void queueRenderEvent(final NativeMapRunnable nativeMapRunnable){
    glSurfaceView.queueEvent(new Runnable() {
      @Override
      public void run() {
        nativeMapRunnable.execute(nativeMapView);
      }
    });
  }

  void queueUiEvent(Runnable runnable){
    glSurfaceView.post(runnable);
  }

  NativeMapView getNativeMapView(){
    return nativeMapView;
  }

  interface NativeMapRunnable {
    @WorkerThread
    void execute(@NonNull NativeMapView nativeMapView);
  }
}
