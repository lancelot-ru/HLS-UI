#include "mainwindow.h"

#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QStatusBar>
#include <QTableView>
#include <QVBoxLayout>

#include "backend.h"
#include "utils.h"

class MainWindow::Impl : public QObject
{
public:
    MainWindow *mParent;

    Backend *mBackend;

    QLineEdit *mUrlLineEdit;
    QLineEdit *mBitratePercentEdit;
    QPushButton *mAnalyseButton;
    QTableView *mAudioView;
    QTableView *mVideoView;
    QTableView *mLogView;

    Impl(MainWindow *parent)
        : mParent(parent)
        , mBackend(new Backend(parent))
    {
        mParent->statusBar();

        connect(mBackend, &Backend::analysisFinished, this, &Impl::onAnalysisFinished);
        connect(mBackend, &Backend::error, this, &Impl::onErrorOccured);

        createWidgets();
    }

    void createWidgets()
    {
        mUrlLineEdit = new QLineEdit(mParent);
        mUrlLineEdit->setPlaceholderText("Введите URL");
        mUrlLineEdit->setText("http://devstreaming-cdn.apple.com/videos/streaming/examples/img_bipbop_adv_example_ts/master.m3u8");

        mBitratePercentEdit = new QLineEdit(mParent);
        mBitratePercentEdit->setPlaceholderText("Введите допустимое отклонение битрейта в процентах (по умолчанию - 10%)");

        mAnalyseButton = new QPushButton("Анализировать", mParent);
        connect(mAnalyseButton, &QPushButton::clicked, this, &Impl::onAnalyseButtonClicked);

        mAudioView = new QTableView(mParent);
        mAudioView->setModel(mBackend->audioModel());
        mAudioView->resizeColumnsToContents();
        mAudioView->horizontalHeader()->setStretchLastSection(true);
        mAudioView->setEditTriggers(QAbstractItemView::NoEditTriggers);
        mAudioView->setSelectionBehavior(QAbstractItemView::SelectRows);
        mAudioView->setSelectionMode(QAbstractItemView::SingleSelection);
        mAudioView->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);

        mVideoView = new QTableView(mParent);
        mVideoView->setModel(mBackend->videoModel());
        mVideoView->resizeColumnsToContents();
        mVideoView->horizontalHeader()->setStretchLastSection(true);
        mVideoView->setEditTriggers(QAbstractItemView::NoEditTriggers);
        mVideoView->setSelectionBehavior(QAbstractItemView::SelectRows);
        mVideoView->setSelectionMode(QAbstractItemView::SingleSelection);
        mVideoView->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);

        mLogView = new QTableView(mParent);
        mLogView->setModel(mBackend->logModel());
        mLogView->horizontalHeader()->setStretchLastSection(true);
        mLogView->setEditTriggers(QAbstractItemView::NoEditTriggers);
        mLogView->setSelectionBehavior(QAbstractItemView::SelectRows);
        mLogView->setSelectionMode(QAbstractItemView::SingleSelection);
        mLogView->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
        connect(mLogView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &Impl::onLogSelectionChanged);

        QSplitter *splitter = new QSplitter(Qt::Vertical, mParent);
        splitter->addWidget(mVideoView);
        splitter->addWidget(mAudioView);       
        splitter->setSizes(QList<int>({INT_MAX, INT_MAX}));

        QVBoxLayout *layout = new QVBoxLayout();
        layout->setSpacing(10);
        layout->setMargin(10);
        layout->addWidget(splitter);
        layout->addWidget(mUrlLineEdit);
        layout->addWidget(mBitratePercentEdit);
        layout->addWidget(mAnalyseButton, 0, Qt::AlignRight);

        QVBoxLayout *logLayout = new QVBoxLayout();
        logLayout->setSpacing(10);
        logLayout->setMargin(10);
        logLayout->addWidget(mLogView);

        QWidget *widget1 = new QWidget();
        widget1->setLayout(layout);

        QWidget *widget2 = new QWidget();
        widget2->setLayout(logLayout);

        QSplitter *mainSplitter = new QSplitter(Qt::Horizontal, mParent);
        mainSplitter->addWidget(widget1);
        mainSplitter->addWidget(widget2);
        mainSplitter->setSizes(QList<int>({850, 450}));

        mParent->setCentralWidget(mainSplitter);
    }

    void onAnalyseButtonClicked()
    {
        if( mUrlLineEdit->text().isEmpty() )
        {
            QMessageBox::warning(mParent, "Ошибка!", "Пустое поле URL. Введите URL!");
            return;
        }
        if( !mBitratePercentEdit->text().isEmpty() )
        {
            mBackend->setDeviation(mBitratePercentEdit->text().toDouble());
        }
        else
        {
            mBackend->setDeviation(10);
        }

        mBackend->reset();

        mParent->statusBar()->showMessage("Ждите...");
        mBackend->parseUrl(mUrlLineEdit->text());
    }

    void onLogSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
    {
        Q_UNUSED(deselected)

        mVideoView->selectionModel()->clearSelection();
        mAudioView->selectionModel()->clearSelection();

        if( !selected.isEmpty() )
        {
            int row = selected.indexes().first().row();
            QModelIndex index = mBackend->logModel()->index(row, 0);
            QString error = index.data().toString();

            auto pair = Utils::getStreamNumbers(error);
            mVideoView->selectRow(pair.first - 1);
            mAudioView->selectRow(pair.second - 1);
        }
    }

    void onAnalysisFinished()
    {
        mParent->statusBar()->clearMessage();
        mAudioView->resizeColumnsToContents();
        mVideoView->resizeColumnsToContents();
    }

    void onErrorOccured(const QString &error)
    {
        mParent->statusBar()->showMessage("Ошибка!");
        QMessageBox::warning(mParent, "Ошибка!", error);
        mParent->statusBar()->clearMessage();
    }
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , _impl (new Impl(this))
{

}
