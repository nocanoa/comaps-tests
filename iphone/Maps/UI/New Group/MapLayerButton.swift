import SwiftUI

/// View for a map layer button
struct MapLayerButton: View {
    // MARK: Properties
    
    /// The actual view
    var body: some View {
        Group {
            if Settings.leftMainButtonKind != .layers {
                Button {
                    if MapOverlayManager.trafficEnabled() || MapOverlayManager.transitEnabled() || MapOverlayManager.isoLinesEnabled() || MapOverlayManager.outdoorEnabled() {
                        MapOverlayManager.setTrafficEnabled(false)
                        MapOverlayManager.setTransitEnabled(false)
                        MapOverlayManager.setIsoLinesEnabled(false)
                        MapOverlayManager.setOutdoorEnabled(false)
                    } else {
                        MWMMapViewControlsManager.manager()?.menuState = .layers
                    }
                } label: {
                    Label("Show Layers", systemImage: "square.stack.3d.up.fill")
                }
                .buttonStyle(FloatingButtonStyle())
            }
        }
    }
}
