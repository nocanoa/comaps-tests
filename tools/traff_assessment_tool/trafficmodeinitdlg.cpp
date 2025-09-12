#include "traff_assessment_tool/trafficmodeinitdlg.h"
#include "ui_trafficmodeinitdlg.h"

#include "platform/settings.hpp"

#include <QtWidgets/QFileDialog>
#include <QFileInfo>

#include <string>

namespace
{
std::string const kDataFilePath = "LastTraffAssessmentDataFilePath";
}  // namespace

namespace traffxml
{
TrafficModeInitDlg::TrafficModeInitDlg(QWidget * parent) :
  QDialog(parent),
  m_ui(new Ui::TrafficModeInitDlg)
{
  m_ui->setupUi(this);

  QString directory = {};
  std::string lastDataFilePath;
  if (settings::Get(kDataFilePath, lastDataFilePath))
  {
    m_ui->dataFileName->setText(QString::fromStdString(lastDataFilePath));
    directory = QFileInfo(QString::fromStdString(lastDataFilePath)).absolutePath();
  }

  connect(m_ui->chooseDataFileButton, &QPushButton::clicked, [this, directory](bool) {
      SetFilePathViaDialog(*m_ui->dataFileName, tr("Choose data file"), directory, "*.xml");
  });
}

TrafficModeInitDlg::~TrafficModeInitDlg()
{
  delete m_ui;
}

void TrafficModeInitDlg::accept()
{
  m_dataFileName = m_ui->dataFileName->text().trimmed().toStdString();
  settings::Set(kDataFilePath, m_dataFileName);
  QDialog::accept();
}

void TrafficModeInitDlg::SetFilePathViaDialog(QLineEdit & dest, QString const & title,
                                              QString const & directory, QString const & filter)
{
  QFileDialog openFileDlg(nullptr, title, directory, filter);
  openFileDlg.exec();
  if (openFileDlg.result() != QDialog::DialogCode::Accepted)
    return;

  dest.setText(openFileDlg.selectedFiles().first());
}
}  // namespace traffxml
