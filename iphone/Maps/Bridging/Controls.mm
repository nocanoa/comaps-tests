#import "Controls.h"
#import "SwiftBridge.h"

#include <CoreApi/Framework.h>

@implementation Controls

static Controls *shared = nil;

+ (Controls *)shared
{
  if (shared == nil)
  {
    shared = [[super allocWithZone:NULL] init];
  }
  
  return shared;
}

+ (id)allocWithZone:(NSZone *)zone
{
  return [self shared];
}

- (id)copyWithZone:(NSZone *)zone
{
  return self;
}

- (id)init
{
  self = [super init];
  if (self != nil)
  {
    // Initialize instance variables here
  }
  return self;
}

+ (void)zoomIn;
{
    GetFramework().Scale(Framework::SCALE_MAG, true);
}

+ (void)zoomOut;
{
    GetFramework().Scale(Framework::SCALE_MIN, true);
}

+ (NSString *)positionModeRawValue;
{
    location::EMyPositionMode mode = GetFramework().GetMyPositionMode();
    switch (mode)
    {
        case location::EMyPositionMode::NotFollowNoPosition: return @"Locate";
        case location::EMyPositionMode::NotFollow: return @"Locate";
        case location::EMyPositionMode::PendingPosition: return @"Locating";
        case location::EMyPositionMode::Follow: return @"Following";
        case location::EMyPositionMode::FollowAndRotate: return @"FollowingAndRotated";
    }
    return @"Locate";
}

+ (void)switchToNextPositionMode;
{
    [MWMLocationManager enableLocationAlert];
    GetFramework().SwitchMyPositionNextMode();
}

- (bool)hasMainButtons;
{
    return true;
}

@end
