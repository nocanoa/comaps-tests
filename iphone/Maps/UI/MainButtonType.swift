import SwiftUI

extension MainButton {
    /// The type of the left bottom bar button
    enum Kind: String, Codable, CaseIterable, Identifiable {
        case hidden = "Hidden"
        case addPlace = "AddPlace"
        case recordTrack = "RecordTrack"
        case search = "Search"
        case bookmarks = "Bookmarks"
        case settings = "Settings"
        case help = "Help"
        case more = "More"
        case layers = "Layers"
        
        
        
        // MARK: Properties
        
        /// The configurable cases
        static var configurableCases: [MainButton.Kind] {
            allCases.filter { kind in
                return kind != .more && kind != .bookmarks && kind != .search
            }
        }
        
        
        /// The id
        var id: Self { self }
        
        
        /// The description text
        var description: String {
            switch self {
                case .hidden:
                    return String(localized: "disabled")
                case .addPlace:
                    return String(localized: "placepage_add_place_button")
                case .recordTrack:
                    return String(localized: "start_track_recording")
                case .search:
                    return String(localized: "search")
                case .bookmarks:
                    return String(localized: "bookmarks")
                case .settings:
                    return String(localized: "settings")
                case .help:
                    return String(localized: "help")
                case .more:
                    return String(localized: "placepage_more_button")
                case .layers:
                    return String(localized: "layers_title")
            }
        }
        
        
        /// The image
        var image: Image? {
            switch self {
                case .addPlace:
                    return Image(systemName: "plus")
                case .recordTrack:
                    return Image(.MainButtons.LeftButton.recordTrack)
                case .search:
                    return Image(systemName: "magnifyingglass")
                case .bookmarks:
                    return Image(systemName: "list.star")
                case .settings:
                    return Image(systemName: "gearshape.fill")
                case .help:
                    return Image(systemName: "info.circle")
                case .layers:
                    return Image(systemName: "square.stack.3d.up.fill")
                case .more:
                    return Image(systemName: "ellipsis.circle")
                default:
                    return nil
            }
        }
    }
}
