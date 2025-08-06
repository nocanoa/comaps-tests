import SwiftUI

struct FloatingButtonStyle: ButtonStyle {
    /// If the button is round
    var isRound: Bool = true
    
    
    func makeBody(configuration: Configuration) -> some View {
        configuration.label
        .labelStyle(.iconOnly)
        .padding(10)
        .aspectRatio(1, contentMode: .fill)
        .background {
            if isRound {
                Circle()
                .stroke(Color.white.opacity(0.7), lineWidth: 1)
                .background {
                    Color.white.opacity(configuration.isPressed ? 0.7 : 0.8)
                    .clipShape(Circle())
                }
                .aspectRatio(1, contentMode: .fill)
                .shadow(radius: 2)
            } else {
                RoundedRectangle(cornerRadius: 8)
                .stroke(Color.white.opacity(0.7), lineWidth: 1)
                .background {
                    Color.white.opacity(configuration.isPressed ? 0.7 : 0.8)
                    .clipShape(RoundedRectangle(cornerRadius: 8))
                }
                .aspectRatio(1, contentMode: .fill)
                .shadow(radius: 3)
            }
        }
        .font(.title2)
        .foregroundStyle(configuration.role == .destructive ? Color(.BaseColors.red) : Color.secondary)
        .scaleEffect(configuration.isPressed ? (isRound ? 0.96 : 0.98) : 1)
        .animation(.smooth, value: configuration.isPressed)
    }
}
