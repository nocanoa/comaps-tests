extension Controls {
    @objc static let changeChangeTrackRecordingNotificationName: Notification.Name = Notification.Name(rawValue: "ChangeTrackRecording")
    
    /// The notification name for switching position mode
    @objc static let switchPositionModeNotificationName: Notification.Name = Notification.Name(rawValue: "SwitchPositionMode")
    
    @objc static let changeVisibilityMainButtonsNotificationName: Notification.Name = Notification.Name(rawValue: "ChangeVisibilityMainButtons")
    
    static var positionMode: MapPositionButton.Mode {
        return MapPositionButton.Mode(rawValue: Controls.positionModeRawValue()) ?? .locate
    }
}
