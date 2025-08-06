import SwiftUI

extension MapPositionButton {
    /// The mode of the map position button
    enum Mode: String, Codable, CaseIterable, Identifiable {
        case locate = "Locate"
        case locating = "Locating"
        case following = "Following"
        case followingAndRotated = "FollowingAndRotated"
        
        
        
        // MARK: Properties
        
        /// The id
        var id: Self { self }
        
        
        /// The description text
        var description: String {
            switch self {
                case .locate:
                    return String(localized: "Find own location")
                case .locating:
                    return String(localized: "Finding own location...")
                case .following:
                    return String(localized: "Rotate map towards own direction")
                case .followingAndRotated:
                    return String(localized: "Rotate map towards North")
            }
        }
        
        
        /// The image
        var image: Image {
            switch self {
                case .locate:
                    return Image(systemName: "location")
                case .locating:
                    return Image(systemName: "progress.indicator")
                case .following:
                    return Image(systemName: "location.fill")
                case .followingAndRotated:
                    return Image(systemName: "location.north.line.fill")
            }
        }
    }
}
