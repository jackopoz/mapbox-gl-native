#import <XCTest/XCTest.h>
#import <Mapbox/Mapbox.h>

@interface MGLStyleValueTests : XCTestCase
@end

@implementation MGLStyleValueTests

- (void)testStoplessFunction {
    XCTAssertThrowsSpecificNamed([MGLStyleValue<NSNumber *> valueWithInterpolationMode:MGLInterpolationModeExponential cameraStops:@{} options:nil], NSException, NSInvalidArgumentException, @"Stopless function should raise an exception");
}

- (void)testDeprecatedFunctions {
    MGLShapeSource *shapeSource = [[MGLShapeSource alloc] initWithIdentifier:@"test" shape:nil options:nil];
    MGLSymbolStyleLayer *symbolStyleLayer = [[MGLSymbolStyleLayer alloc] initWithIdentifier:@"symbolLayer" source:shapeSource];
    
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    // deprecated function, stops with float values
    NSDictionary<NSNumber *, MGLStyleValue<NSNumber *> *> *stops = @{
        @1: [MGLStyleValue<NSNumber *> valueWithRawValue:@0],
        @2: [MGLStyleValue<NSNumber *> valueWithRawValue:@1],
        @3: [MGLStyleValue<NSNumber *> valueWithRawValue:@2],
        @4: [MGLStyleValue<NSNumber *> valueWithRawValue:@0],
    };
    MGLStyleValue<NSNumber *> *iconHaloBlurStyleValue = [MGLStyleValue<NSNumber *> valueWithInterpolationBase:1.0 stops:stops];
    symbolStyleLayer.iconHaloBlur = iconHaloBlurStyleValue;
    XCTAssertEqualObjects(symbolStyleLayer.iconHaloBlur, iconHaloBlurStyleValue);
    
    // deprecated function, stops with boolean values
    stops = @{
        @1: [MGLStyleValue<NSNumber *> valueWithRawValue:@YES],
        @2: [MGLStyleValue<NSNumber *> valueWithRawValue:@NO],
        @3: [MGLStyleValue<NSNumber *> valueWithRawValue:@YES],
        @4: [MGLStyleValue<NSNumber *> valueWithRawValue:@NO],
    };
    MGLStyleValue<NSNumber *> *iconAllowsOverlapStyleValue = [MGLStyleValue<NSNumber *> valueWithInterpolationBase:1.0 stops:stops];
    symbolStyleLayer.iconAllowsOverlap = iconAllowsOverlapStyleValue;
    // iconAllowsOverlap is boolean so mgl and mbgl conversions will coerce the developers stops into interval stops
    MGLStyleValue<NSNumber *> *expectedIconAllowsOverlapStyleValue = [MGLStyleValue<NSNumber *> valueWithInterpolationMode:MGLInterpolationModeInterval cameraStops:stops options:nil];
    XCTAssertEqualObjects(symbolStyleLayer.iconAllowsOverlap, expectedIconAllowsOverlapStyleValue);
#pragma clang diagnostic pop
}

@end
