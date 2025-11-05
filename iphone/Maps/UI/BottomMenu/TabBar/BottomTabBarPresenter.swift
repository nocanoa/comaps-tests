protocol BottomTabBarPresenterProtocol: AnyObject {
  func configure()
  func onLeftButtonPressed()
  func onSearchButtonPressed()
  func onBookmarksButtonPressed()
  func onMenuButtonPressed()
}

class BottomTabBarPresenter: NSObject {
  private let interactor: BottomTabBarInteractorProtocol
  
  init(interactor: BottomTabBarInteractorProtocol) {
    self.interactor = interactor
  }
}

extension BottomTabBarPresenter: BottomTabBarPresenterProtocol {
  func configure() {
  }
    
  func onLeftButtonPressed() {
    interactor.openLeftButton()
  }

  func onSearchButtonPressed() {
    interactor.openSearch()
  }

  func onBookmarksButtonPressed() {
    interactor.openBookmarks()
  }

  func onMenuButtonPressed() {
    interactor.openMenu()
  }
}

