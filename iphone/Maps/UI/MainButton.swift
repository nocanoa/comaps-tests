import SwiftUI

/// View for a main button
struct MainButton: View {
    // MARK: Properties
    
    /// The kind
    var kind: MainButton.Kind
    
    
    /// The actual view
    var body: some View {
        if kind != .hidden {
            Button {
                if kind == .addPlace {
                    if let controlsManager = MWMMapViewControlsManager.manager() as? BottomMenuDelegate {
                        controlsManager.addPlace()
                    }
                } else if kind == .recordTrack {
                    let trackRecorder: TrackRecordingManager = .shared
                    switch trackRecorder.recordingState {
                        case .active:
                            MapViewController.shared()?.showTrackRecordingPlacePage()
                        case .inactive:
                            trackRecorder.start { result in
                                switch result {
                                    case .success:
                                        MapViewController.shared()?.showTrackRecordingPlacePage()
                                    case .failure:
                                        break
                                }
                            }
                    }
                } else if kind == .search {
                    if let searchManager = MapViewController.shared()?.searchManager {
                        if searchManager.isSearching {
                            searchManager.close()
                        } else {
                            searchManager.startSearching(isRouting: false)
                        }
                    }
                } else if kind == .bookmarks {
                    MapViewController.shared()?.bookmarksCoordinator.open()
                } else if kind == .settings {
                    MapViewController.shared()?.openSettings()
                } else if kind == .help {
                    MapViewController.shared()?.openAbout()
                } else if kind == .more {
                    if let controlsManager = MWMMapViewControlsManager.manager() {
                        if controlsManager.menuState == .active {
                            controlsManager.menuState = .inactive
                        } else if controlsManager.menuState == .inactive {
                            controlsManager.menuState = .active
                        }
                    }
                } else if kind == .layers {
                    if MapOverlayManager.trafficEnabled() || MapOverlayManager.transitEnabled() || MapOverlayManager.isoLinesEnabled() || MapOverlayManager.outdoorEnabled() {
                        MapOverlayManager.setTrafficEnabled(false)
                        MapOverlayManager.setTransitEnabled(false)
                        MapOverlayManager.setIsoLinesEnabled(false)
                        MapOverlayManager.setOutdoorEnabled(false)
                    } else {
                        MWMMapViewControlsManager.manager()?.menuState = .layers
                    }
                }
            } label: {
                Label {
                    Text(kind.description)
                } icon: {
                    if kind != .layers, let image = kind.image {
                        image
                    }
                }
            }
            .buttonStyle(FloatingButtonStyle(isRound: false))
            .frame(minWidth: 44, idealWidth: 44, minHeight: 44, idealHeight: 44)
        }
    }
}
