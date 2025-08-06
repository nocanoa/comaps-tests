import SwiftUI

/// View for a map position mode button
struct MapPositionButton: View {
    // MARK: Properties
    
    /// The mode
    @State private var mode: MapPositionButton.Mode = .locate
    
    
    /// The publisher to know when to stop showing the Safari view for the login form
    private let switchPositionModePublisher = NotificationCenter.default.publisher(for: Controls.switchPositionModeNotificationName)
    
    
    /// The actual view
    var body: some View {
        Button {
            Controls.switchToNextPositionMode()
        } label: {
            Label {
                Text(mode.description)
            } icon: {
                if mode == .following || mode == .followingAndRotated {
                    mode.image
                        .foregroundStyle(Color.BaseColors.blue)
                } else {
                    mode.image
                }
            }
        }
        .buttonStyle(FloatingButtonStyle())
        .onAppear {
            mode = Controls.positionMode
        }
        .onReceive(switchPositionModePublisher) { _ in
            mode = Controls.positionMode
        }
    }
}
