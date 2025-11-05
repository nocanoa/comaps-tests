import CarPlay

final class SettingsTemplateBuilder {
  // MARK: - CPGridTemplate builder
  class func buildGridTemplate() -> CPGridTemplate {
    let actions = SettingsTemplateBuilder.buildGridButtons()
    let gridTemplate = CPGridTemplate(title: L("settings"),
                                      gridButtons: actions)
    
    return gridTemplate
  }
  
  private class func buildGridButtons() -> [CPGridButton] {
    let options = RoutingOptions()
    return [createUnpavedButton(options: options),
            createTollButton(options: options),
            createFerryButton(options: options),
            createStepsButton(options: options),
            createSpeedcamButton()]
  }
  
  // MARK: - CPGridButton builders
  private class func createTollButton(options: RoutingOptions) -> CPGridButton {
    var tollIconName = "options.tolls"
    if options.avoidToll { tollIconName += ".slash" }
    let configuration = UIImage.SymbolConfiguration(textStyle: .title1)
    var image = UIImage(named: tollIconName, in: nil, with: configuration)!
    if #unavailable(iOS 26) {
      image = image.withTintColor(.white, renderingMode: .alwaysTemplate)
      image = UIImage(data: image.pngData()!)!.withRenderingMode(.alwaysTemplate)
    }
    let tollButton = CPGridButton(titleVariants: [L("avoid_tolls")], image: image) { _ in
                                    options.avoidToll = !options.avoidToll
                                    options.save()
                                    CarPlayService.shared.updateRouteAfterChangingSettings()
                                    CarPlayService.shared.popTemplate(animated: true)
    }
    return tollButton
  }
  
  private class func createUnpavedButton(options: RoutingOptions) -> CPGridButton {
    var unpavedIconName = "options.unpaved"
    if options.avoidDirty { unpavedIconName += ".slash" }
    let configuration = UIImage.SymbolConfiguration(textStyle: .title1)
    var image = UIImage(named: unpavedIconName, in: nil, with: configuration)!
    if #unavailable(iOS 26) {
      image = image.withTintColor(.white, renderingMode: .alwaysTemplate)
      image = UIImage(data: image.pngData()!)!.withRenderingMode(.alwaysTemplate)
    }
    let unpavedButton = CPGridButton(titleVariants: [L("avoid_unpaved")], image: image) { _ in
                                      options.avoidDirty = !options.avoidDirty
                                      options.save()
                                      CarPlayService.shared.updateRouteAfterChangingSettings()
                                      CarPlayService.shared.popTemplate(animated: true)
    }
    return unpavedButton
  }
  
  private class func createFerryButton(options: RoutingOptions) -> CPGridButton {
    var ferryIconName = "options.ferries"
    if options.avoidFerry { ferryIconName += ".slash" }
    let configuration = UIImage.SymbolConfiguration(textStyle: .title1)
    var image = UIImage(named: ferryIconName, in: nil, with: configuration)!
    if #unavailable(iOS 26) {
      image = image.withTintColor(.white, renderingMode: .alwaysTemplate)
      image = UIImage(data: image.pngData()!)!.withRenderingMode(.alwaysTemplate)
    }
    let ferryButton = CPGridButton(titleVariants: [L("avoid_ferry")], image: image) { _ in
                                    options.avoidFerry = !options.avoidFerry
                                    options.save()
                                    CarPlayService.shared.updateRouteAfterChangingSettings()
                                    CarPlayService.shared.popTemplate(animated: true)
    }
    return ferryButton
  }
    
  private class func createStepsButton(options: RoutingOptions) -> CPGridButton {
    var stepsIconName = "options.steps"
    if options.avoidSteps { stepsIconName += ".slash" }
    let configuration = UIImage.SymbolConfiguration(textStyle: .title1)
    var image = UIImage(named: stepsIconName, in: nil, with: configuration)!
    if #unavailable(iOS 26) {
      image = image.withTintColor(.white, renderingMode: .alwaysTemplate)
      image = UIImage(data: image.pngData()!)!.withRenderingMode(.alwaysTemplate)
    }
    let stepsButton = CPGridButton(titleVariants: [L("avoid_steps")], image: image) { _ in
                                    options.avoidSteps = !options.avoidSteps
                                    options.save()
                                    CarPlayService.shared.updateRouteAfterChangingSettings()
                                    CarPlayService.shared.popTemplate(animated: true)
    }
    return stepsButton
  }
  
  private class func createSpeedcamButton() -> CPGridButton {
    var speedcamIconName = "options.speedcamera"
    let isSpeedCamActivated = CarPlayService.shared.isSpeedCamActivated
    if !isSpeedCamActivated { speedcamIconName += ".slash" }
    let configuration = UIImage.SymbolConfiguration(textStyle: .title1)
    var image = UIImage(named: speedcamIconName, in: nil, with: configuration)!
    if #unavailable(iOS 26) {
      image = image.withTintColor(.white, renderingMode: .alwaysTemplate)
      image = UIImage(data: image.pngData()!)!.withRenderingMode(.alwaysTemplate)
    }
    let speedButton = CPGridButton(titleVariants: [L("speedcams_alert_title_carplay_1"), L("speedcams_alert_title_carplay_2")], image: image) { _ in
                                    CarPlayService.shared.isSpeedCamActivated = !isSpeedCamActivated
                                    CarPlayService.shared.popTemplate(animated: true)
    }
    return speedButton
  }
}
