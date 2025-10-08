
class BottomTabBarViewController: UIViewController {
  var presenter: BottomTabBarPresenterProtocol!

  @IBOutlet var leftButton: MWMButton?
  @IBOutlet var searchButton: MWMButton?
  @IBOutlet var searchConstraintWithLeftButton: NSLayoutConstraint?
  @IBOutlet var searchConstraintWithoutLeftButton: NSLayoutConstraint?
  @IBOutlet var bookmarksButton: MWMButton?
  @IBOutlet var bookmarksConstraintWithLeftButton: NSLayoutConstraint?
  @IBOutlet var bookmarksConstraintWithoutLeftButton: NSLayoutConstraint?
  @IBOutlet var moreButton: MWMButton?
  @IBOutlet var downloadBadge: UIView?
  
  private var avaliableArea = CGRect.zero
  @objc var isHidden: Bool = false {
    didSet {
      updateFrame(animated: true)
    }
  }
  @objc var isApplicationBadgeHidden: Bool = true {
    didSet {
      updateBadge()
    }
  }
  var tabBarView: BottomTabBarView {
    return view as! BottomTabBarView
  }
  @objc static var controller: BottomTabBarViewController? {
    return MWMMapViewControlsManager.manager()?.tabBarController
  }
  
  override func viewDidLoad() {
    super.viewDidLoad()
    presenter.configure()
    
    NotificationCenter.default.addObserver(forName: UserDefaults.didChangeNotification, object: nil, queue: nil) { _ in
      DispatchQueue.main.async {
        self.updateLeftButton()
      }
    }
  }
  
  override func viewWillAppear(_ animated: Bool) {
    super.viewWillAppear(animated)
    
    leftButton?.imageView?.contentMode = .scaleAspectFit
    updateBadge()
  }
  
  override func viewWillLayoutSubviews() {
    super.viewWillLayoutSubviews()
    updateLeftButton()
  }
  
  static func updateAvailableArea(_ frame: CGRect) {
    BottomTabBarViewController.controller?.updateAvailableArea(frame)
  }
  
  @IBAction func onSearchButtonPressed(_ sender: Any) {
    presenter.onSearchButtonPressed()
  }
  
  @IBAction func onLeftButtonPressed(_ sender: Any) {
    presenter.onLeftButtonPressed()
  }
  
  @IBAction func onBookmarksButtonPressed(_ sender: Any) {
    presenter.onBookmarksButtonPressed()
  }
  
  @IBAction func onMenuButtonPressed(_ sender: Any) {
    presenter.onMenuButtonPressed()
  }
  
  private func updateAvailableArea(_ frame:CGRect) {
    avaliableArea = frame
    updateFrame(animated: false)
    self.view.layoutIfNeeded()
  }
  
  private func updateFrame(animated: Bool) {
    if avaliableArea == .zero {
      return
    }
    let newFrame = CGRect(x: avaliableArea.minX,
                          y: isHidden ? avaliableArea.minY + avaliableArea.height : avaliableArea.minY,
                          width: avaliableArea.width,
                          height: avaliableArea.height)
    let alpha:CGFloat = isHidden ? 0 : 1
    if animated {
      UIView.animate(withDuration: kDefaultAnimationDuration,
                     delay: 0,
                     options: [.beginFromCurrentState],
                     animations: {
        self.view.frame = newFrame
        self.view.alpha = alpha
      }, completion: nil)
    } else {
      self.view.frame = newFrame
      self.view.alpha = alpha
    }
  }
  
  private func updateLeftButton() {
    let leftButtonType = Settings.leftButtonType
    if leftButtonType == .hidden {
      leftButton?.isHidden = true

      if let searchConstraintWithLeftButton, let searchConstraintWithoutLeftButton, let bookmarksConstraintWithLeftButton, let bookmarksConstraintWithoutLeftButton {
        NSLayoutConstraint.activate([searchConstraintWithoutLeftButton, bookmarksConstraintWithoutLeftButton])
        NSLayoutConstraint.deactivate([searchConstraintWithLeftButton, bookmarksConstraintWithLeftButton])
      }
    } else {
      leftButton?.isHidden = false

      leftButton?.setTitle(nil, for: .normal)
      leftButton?.setImage(leftButtonType.image, for: .normal)
      leftButton?.accessibilityLabel = leftButtonType.description;

      if let searchConstraintWithLeftButton, let searchConstraintWithoutLeftButton, let bookmarksConstraintWithLeftButton, let bookmarksConstraintWithoutLeftButton {
        NSLayoutConstraint.activate([searchConstraintWithLeftButton, bookmarksConstraintWithLeftButton])
        NSLayoutConstraint.deactivate([searchConstraintWithoutLeftButton, bookmarksConstraintWithoutLeftButton])
      }
    }
  }
  
  private func updateBadge() {
    downloadBadge?.isHidden = isApplicationBadgeHidden
  }
}
