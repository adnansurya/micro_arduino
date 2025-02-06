#ifndef _MAKROLPGSENSOR_H_
#define _MAKROLPGSENSOR_H_

#include <SinricProDevice.h>
#include <Capabilities/ToggleController.h>
#include <Capabilities/PushNotification.h>
#include <Capabilities/ModeController.h>

class MAKROLPGSensor 
: public SinricProDevice
, public ToggleController<MAKROLPGSensor>
, public PushNotification<MAKROLPGSensor>
, public ModeController<MAKROLPGSensor> {
  friend class ToggleController<MAKROLPGSensor>;
  friend class PushNotification<MAKROLPGSensor>;
  friend class ModeController<MAKROLPGSensor>;
public:
  MAKROLPGSensor(const String &deviceId) : SinricProDevice(deviceId, "MAKROLPGSensor") {};
};

#endif
