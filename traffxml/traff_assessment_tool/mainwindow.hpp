#pragma once

#include "base/string_utils.hpp"

#include <string>

#include <QMainWindow>

class Framework;
class QHBoxLayout;

namespace traffxml
{
class MapWidget;
class TrafficMode;
class WebView;
}

namespace df
{
class DrapeApi;
}

class QDockWidget;

namespace traffxml
{
class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  explicit MainWindow(Framework & framework);

private:
  void CreateTrafficPanel(std::string const & dataFilePath);
  void DestroyTrafficPanel();

  void OnOpenTrafficSample();
  void OnCloseTrafficSample();
  void OnSaveTrafficSample();
  void OnPathEditingStop();

  Framework & m_framework;

  traffxml::TrafficMode * m_trafficMode = nullptr;
  QDockWidget * m_docWidget = nullptr;

#ifdef openlr_obsolete
  QAction * m_goldifyMatchedPathAction = nullptr;
#endif
  QAction * m_saveTrafficSampleAction = nullptr;
  QAction * m_closeTrafficSampleAction = nullptr;
#ifdef openlr_obsolete
  QAction * m_startEditingAction = nullptr;
  QAction * m_commitPathAction  = nullptr;
  QAction * m_cancelPathAction = nullptr;
  QAction * m_ignorePathAction = nullptr;
#endif

  traffxml::MapWidget * m_mapWidget = nullptr;
  QHBoxLayout * m_layout = nullptr;
};
}  // namespace traffxml
