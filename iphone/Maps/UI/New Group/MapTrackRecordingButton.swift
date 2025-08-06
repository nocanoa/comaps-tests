import SwiftUI

/// View for a map track recording button
struct MapTrackRecordingButton: View {
    // MARK: Properties
    
    /// The mode
    @State private var isRecording: Bool = false
    
    
    /// The publisher to know when to stop showing the Safari view for the login form
    private let changeChangeTrackRecordingPublisher = NotificationCenter.default.publisher(for: Controls.changeChangeTrackRecordingNotificationName)
    
    
    /// The actual view
    var body: some View {
        ZStack {
            if isRecording {
                Button(role: .destructive) {
                    if isRecording {
                        MapViewController.shared()?.showTrackRecordingPlacePage()
                    } else {
                        TrackRecordingManager.shared.start { result in
                            switch result {
                                case .success:
                                    MapViewController.shared()?.showTrackRecordingPlacePage()
                                case .failure:
                                    break
                            }
                        }
                    }
                } label: {
                    Label("Show Track Recording", systemImage: "record.circle")
                }
                .buttonStyle(FloatingButtonStyle())
            }
        }
        .onAppear {
            isRecording = (TrackRecordingManager.shared.recordingState == .active)
        }
        .onReceive(changeChangeTrackRecordingPublisher) { _ in
            isRecording = (TrackRecordingManager.shared.recordingState == .active)
        }
    }
}
