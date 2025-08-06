NS_SWIFT_NAME(Controls)
@interface Controls : NSObject

+ (void)zoomIn;
+ (void)zoomOut;
+ (void)switchToNextPositionMode;
+ (NSString *)positionModeRawValue;
+ (Controls *)shared;
- (bool)hasMainButtons;

@end
