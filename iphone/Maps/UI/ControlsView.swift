import SwiftUI

/// View for the interface button
struct ControlsView: View {
    // MARK: Properties
    
    /// The dismiss action of the environment
    @Environment(\.verticalSizeClass) private var verticalSizeClass
    
    
    @State private var hasZoomButtons: Bool = true
    
    
    @State private var leftMainButtonKind: MainButton.Kind = .hidden
    
    
    @State private var hasMainButtons: Bool = true
    
    
    /// The publisher to know when settings changed
    private let settingsPublisher = NotificationCenter.default.publisher(for: UserDefaults.didChangeNotification)
    
    
    /// The publisher to know when settings changed
    private let changeVisibilityMainButtonsPublisher = NotificationCenter.default.publisher(for: Controls.changeVisibilityMainButtonsNotificationName)
    
    
    /// The actual view
    var body: some View {
        ZStack {
            if verticalSizeClass != .compact {
                VStack(alignment: .trailing) {
                    HStack {
                        MapLayerButton()
                        
                        Spacer(minLength: 0)
                        
                        VStack {
                            MapTrackRecordingButton()
                        }
                    }
                    
                    Spacer(minLength: 0)
                    
                    VStack(spacing: 72) {
                        if hasZoomButtons {
                            VStack(spacing: 36) {
                                MapZoomButton(kind: .in)
                                
                                MapZoomButton(kind: .out)
                            }
                        }
                        
                        MapPositionButton()
                    }
                    
                    Spacer(minLength: 0)
                    
                    if hasMainButtons {
                        HStack {
                            if leftMainButtonKind != .hidden {
                                MainButton(kind: leftMainButtonKind)
                                
                                Spacer(minLength: 0)
                            }
                            
                            MainButton(kind: .search)
                            
                            Spacer(minLength: 0)
                            
                            MainButton(kind: .bookmarks)
                            
                            Spacer(minLength: 0)
                            
                            MainButton(kind: .more)
                        }
                        .frame(maxWidth: .infinity)
                    }
                }
                .padding([.leading, .trailing], 10)
            } else {
                HStack {
                    VStack(alignment: .leading) {
                        MapLayerButton()
                        
                        Spacer(minLength: 0)
                        
                        if hasMainButtons {
                            HStack(spacing: 32) {
                                if leftMainButtonKind != .hidden {
                                    MainButton(kind: leftMainButtonKind)
                                }
                                
                                MainButton(kind: .search)
                                
                                MainButton(kind: .bookmarks)
                                
                                MainButton(kind: .more)
                            }
                        }
                    }
                    
                    Spacer(minLength: 0)
                    
                    VStack(alignment: .trailing) {
                        VStack {
                            MapTrackRecordingButton()
                        }
                        
                        Spacer(minLength: 0)
                        
                        if hasZoomButtons {
                            VStack(spacing: 36) {
                                MapZoomButton(kind: .in)
                                
                                MapZoomButton(kind: .out)
                            }
                        }
                        
                        Spacer(minLength: 0)
                        
                        MapPositionButton()
                    }
                }
            }
        }
        .padding(.top, 14)
        .padding(.bottom, 2)
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .onAppear {
            hasZoomButtons = Settings.hasZoomButtons
            leftMainButtonKind = Settings.leftMainButtonKind
            hasMainButtons = Controls.shared().hasMainButtons()
        }
        .onReceive(settingsPublisher) { _ in
            hasZoomButtons = Settings.hasZoomButtons
            leftMainButtonKind = Settings.leftMainButtonKind
        }
        .onReceive(changeVisibilityMainButtonsPublisher) { _ in
            hasMainButtons = Controls.shared().hasMainButtons()
        }
    }
}

