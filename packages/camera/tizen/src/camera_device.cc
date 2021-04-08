// Copyright 2021 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "camera_device.h"

#include <flutter/encodable_value.h>

#include "log.h"

flutter::EncodableValue CameraDevice::GetAvailableCameras() {
  CameraDevice default_camera;
  int count = default_camera.GetDeviceCount();

  flutter::EncodableList cameras;
  for (int i = 0; i < count; i++) {
    flutter::EncodableMap camera;
    camera[flutter::EncodableValue("name")] =
        flutter::EncodableValue("camera" + std::to_string(i + 1));

    int angle = default_camera.GetLensOrientation();
    camera[flutter::EncodableValue("sensorOrientation")] =
        flutter::EncodableValue(angle);
    std::string lensFacing;
    if (i == 0) {
      lensFacing = "back";
    } else {
      lensFacing = "front";
    }
    camera[flutter::EncodableValue("lensFacing")] =
        flutter::EncodableValue(lensFacing);

    cameras.push_back(flutter::EncodableValue(camera));
    default_camera.ChangeCameraDeviceType(CameraDeviceType::kFront);
  }
  return flutter::EncodableValue(cameras);
}

CameraDevice::CameraDevice() {
  CreateCameraHandle();
  state_ = GetState();
}

CameraDevice::CameraDevice(flutter::PluginRegistrar *registrar,
                           FlutterTextureRegistrar *texture_registrar,
                           CameraDeviceType type)
    : registrar_(registrar),
      texture_registrar_(texture_registrar),
      type_(type) {
  CreateCameraHandle();
  texture_id_ = FlutterRegisterExternalTexture(texture_registrar_);
  LOG_DEBUG("texture_id_[%ld]", texture_id_);
  camera_method_channel_ =
      std::make_unique<CameraMethodChannel>(registrar_, texture_id_);
  device_method_channel_ = std::make_unique<DeviceMethodChannel>(registrar_);
  orientation_manager_ = std::make_unique<OrientationManager>(
      device_method_channel_.get(), (OrientationType)GetLensOrientation(),
      type == CameraDeviceType::kFront);

  // Send initial orientation
  auto target_orientation = orientation_manager_->ConvertTargetOrientation(
      OrientationType::kPortraitUp);
  orientation_manager_->SendOrientation(target_orientation);
  orientation_manager_->Start();

  state_ = GetState();
  // Print camera info for convenience of development, These will be removed
  // after release
  PrintState();
  PrintPreviewRotation();
  PrintSupportedPreviewResolution();
}

CameraDevice::~CameraDevice() { Dispose(); }

void CameraDevice::CreateCameraHandle() {
  int error = camera_create((camera_device_e)type_, &handle_);
  LOG_ERROR_IF(error != CAMERA_ERROR_NONE, "camera_create fail - error : %s",
               get_error_message(error));
}

void CameraDevice::DestroyCameraHandle() {
  if (handle_) {
    int error = camera_destroy(handle_);
    LOG_ERROR_IF(error != CAMERA_ERROR_NONE, "camera_destroy fail - error : %s",
                 get_error_message(error));
    handle_ = nullptr;
  }
}

void CameraDevice::ChangeCameraDeviceType(CameraDeviceType type) {
  int error = camera_change_device(handle_, (camera_device_e)type);
  LOG_ERROR_IF(error != CAMERA_ERROR_NONE,
               "camera_change_device fail - error : %s",
               get_error_message(error));
  type_ = type;
}

void CameraDevice::Dispose() {
  LOG_DEBUG("enter");
  if (state_ == CameraDeviceState::kPreview) {
    StopPreview();
    UnsetMediaPacketPreviewCb();
  }

  DestroyCameraHandle();

  if (orientation_manager_) {
    orientation_manager_->Stop();
  }

  if (texture_registrar_) {
    FlutterUnregisterExternalTexture(texture_registrar_, texture_id_);
    texture_registrar_ = nullptr;
  }
}

int CameraDevice::GetDeviceCount() {
  int count = 0;
  // If the device supports primary and secondary camera, this returns 2. If 1
  // is returned, the device only supports primary camera.
  int error = camera_get_device_count(handle_, &count);
  LOG_ERROR_IF(error != CAMERA_ERROR_NONE,
               "camera_get_device_count fail - error : %s",
               get_error_message(error));

  LOG_DEBUG("count[%d]", count);
  return count;
}

int CameraDevice::GetLensOrientation() {
  int angle = 0;
  int error = camera_attr_get_lens_orientation(handle_, &angle);
  LOG_ERROR_IF(error != CAMERA_ERROR_NONE,
               "camera_attr_get_lens_orientation fail - error : %s",
               get_error_message(error));

  LOG_DEBUG("angle[%d]", angle);
  return angle;
}

CameraDeviceState CameraDevice::GetState() {
  camera_state_e state;
  int error = camera_get_state(handle_, &state);
  LOG_ERROR_IF(error != CAMERA_ERROR_NONE, "camera_get_state fail - error : %s",
               get_error_message(error));
  return (CameraDeviceState)state;
}

void CameraDevice::PrintSupportedPreviewResolution() {
  LOG_DEBUG("enter");
  int error = camera_foreach_supported_preview_resolution(
      handle_,
      [](int width, int height, void *user_data) -> bool {
        LOG_DEBUG("supported preview w[%d] h[%d]", width, height);
        return true;
      },
      nullptr);

  LOG_ERROR_IF(error != CAMERA_ERROR_NONE,
               "camera_foreach_supported_preview_resolution fail - error : %s",
               get_error_message(error));
}

void CameraDevice::PrintState() {
  switch (state_) {
    case CameraDeviceState::kNone:
      LOG_DEBUG("CameraDeviceState[None]");
      break;
    case CameraDeviceState::kCreated:
      LOG_DEBUG("CameraDeviceState[Created]");
      break;
    case CameraDeviceState::kPreview:
      LOG_DEBUG("CameraDeviceState[Preview]");
      break;
    case CameraDeviceState::kCapturing:
      LOG_DEBUG("CameraDeviceState[Capturing]");
      break;
    case CameraDeviceState::kCaputred:
      LOG_DEBUG("CameraDeviceState[Caputred]");
      break;
    default:
      LOG_DEBUG("CameraDeviceState[Unknown]");
      break;
  }
}

void CameraDevice::PrintPreviewRotation() {
  camera_rotation_e val;
  int error = camera_attr_get_stream_rotation(handle_, &val);
  switch (val) {
    case CAMERA_ROTATION_NONE:
      LOG_DEBUG("CAMERA_ROTATION_NONE");
      break;
    case CAMERA_ROTATION_90:
      LOG_DEBUG("CAMERA_ROTATION_90");
      break;
    case CAMERA_ROTATION_180:
      LOG_DEBUG("CAMERA_ROTATION_180");
      break;
    case CAMERA_ROTATION_270:
      LOG_DEBUG("CAMERA_ROTATION_270");
      break;
    default:
      break;
  }
}

Size CameraDevice::GetRecommendedPreviewResolution() {
  Size preview_size;
  int w, h;
  int error = camera_get_recommended_preview_resolution(handle_, &w, &h);
  LOG_ERROR_IF(error != CAMERA_ERROR_NONE,
               "camera_get_recommended_preview_resolution fail - error : %s",
               get_error_message(error));

  auto target_orientation = orientation_manager_->ConvertTargetOrientation(
      OrientationType::kPortraitUp);
  if (target_orientation == OrientationType::kLandscapeLeft ||
      target_orientation == OrientationType::kLandscapeRight) {
    preview_size.width = h;
    preview_size.height = w;
  } else {
    preview_size.width = w;
    preview_size.height = h;
  }

  LOG_DEBUG("width[%f] height[%f]", preview_size.width, preview_size.height);
  return preview_size;
}

bool CameraDevice::Open(std::string /* TODO : image_format_group */) {
  LOG_DEBUG("enter");
  SetMediaPacketPreviewCb([](media_packet_h pkt, void *data) {
    tbm_surface_h surface = nullptr;
    int error = media_packet_get_tbm_surface(pkt, &surface);
    LOG_ERROR_IF(error != MEDIA_PACKET_ERROR_NONE,
                 "media_packet_get_tbm_surface fail - error : %s",
                 get_error_message(error));

    if (error == 0) {
      CameraDevice *camera_device = (CameraDevice *)data;
      FlutterMarkExternalTextureFrameAvailable(
          camera_device->GetTextureRegistrar(), camera_device->GetTextureId(),
          surface);
    }

    // destroy packet
    if (pkt) {
      error = media_packet_destroy(pkt);
      LOG_ERROR_IF(error != MEDIA_PACKET_ERROR_NONE,
                   "media_packet_destroy fail - error : %s",
                   get_error_message(error));
    }
  });

  StartPreview();

  flutter::EncodableMap map;
  Size size = GetRecommendedPreviewResolution();
  map[flutter::EncodableValue("previewWidth")] =
      flutter::EncodableValue(size.width);
  map[flutter::EncodableValue("previewHeight")] =
      flutter::EncodableValue(size.height);

  // TODO
  map[flutter::EncodableValue("exposureMode")] =
      flutter::EncodableValue("auto");
  map[flutter::EncodableValue("focusMode")] = flutter::EncodableValue("auto");
  map[flutter::EncodableValue("exposurePointSupported")] =
      flutter::EncodableValue(false);
  map[flutter::EncodableValue("focusPointSupported")] =
      flutter::EncodableValue(false);

  auto value = std::make_unique<flutter::EncodableValue>(map);
  camera_method_channel_->Send(CameraEventType::kInitialized, std::move(value));
  return true;
}

bool CameraDevice::SetMediaPacketPreviewCb(MediaPacketPreviewCb callback) {
  int error = camera_set_media_packet_preview_cb(handle_, callback, this);
  RETV_LOG_ERROR_IF(error != CAMERA_ERROR_NONE, false,
                    "camera_set_media_packet_preview_cb fail - error : %s",
                    get_error_message(error));

  return true;
}

bool CameraDevice::SetPreviewSize(Size size) {
  int w, h;
  w = (int)size.width;
  h = (int)size.height;

  LOG_DEBUG("camera_set_preview_resolution w[%d] h[%d]", w, h);

  int error = camera_set_preview_resolution(handle_, w, h);
  RETV_LOG_ERROR_IF(error != CAMERA_ERROR_NONE, false,
                    "camera_set_preview_resolution fail - error : %s",
                    get_error_message(error));
  return true;
}

bool CameraDevice::UnsetMediaPacketPreviewCb() {
  int error = camera_unset_media_packet_preview_cb(handle_);
  RETV_LOG_ERROR_IF(error != CAMERA_ERROR_NONE, false,
                    "camera_unset_media_packet_preview_cb fail - error : %s",
                    get_error_message(error));

  return true;
}

bool CameraDevice::StartPreview() {
  int error = camera_start_preview(handle_);
  RETV_LOG_ERROR_IF(error != CAMERA_ERROR_NONE, false,
                    "camera_start_preview fail - error : %s",
                    get_error_message(error));

  state_ = GetState();
  return true;
}

bool CameraDevice::StopPreview() {
  int error = camera_stop_preview(handle_);
  RETV_LOG_ERROR_IF(error != CAMERA_ERROR_NONE, false,
                    "camera_stop_preview fail - error : %s",
                    get_error_message(error));

  state_ = GetState();
  return true;
}
