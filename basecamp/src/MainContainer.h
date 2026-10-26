#pragma once

#include "IShellHost.h"

#include <QWidget>
#include <QHBoxLayout>
#include <QPointer>
#include <QStackedWidget>

class QQuickWidget;
class WorkspaceArea;
class DesktopArea;
class AppWindow;
class ShellPrefs;
class DesktopStatusBar;
class DesktopRoot;
class ShortcutBridge;
class TabArea;
class QKeyEvent;

// MainContainer — the UI shell. Holds exactly one host-side pointer, an
// IShellHost*: no LogosAPI*, no QtLogosCore*, no MainUIBackend*. QML reaches
// the backend as an opaque QObject* via IShellHost::backendObject(). That is
// what lets this class, and everything below it, compile against Qt alone.
class MainContainer : public QWidget, public IShellObserver
{
    Q_OBJECT

public:
    // `host` is borrowed and outlives this widget. Must not be null.
    explicit MainContainer(IShellHost* host, QWidget* parent = nullptr);
    ~MainContainer();

    WorkspaceArea* getWorkspaceArea() const { return m_workspaceArea; }

    // Stops the host from calling back into this object. Idempotent:
    // MainShellView::destroyShell() calls it before deleting, and the
    // destructor calls it again for the path where Qt's parent-child teardown
    // gets here first.
    void detachFromHost();

    // ── IShellObserver ──────────────────────────────────────────────────────
    void onSectionIndexChanged(int index) override;
    void onNavigateToApps() override;
    void onPluginWindowRequested(QWidget* widget, const QString& title) override;
    void onPluginWindowRemoveRequested(QWidget* widget) override;
    // Presentation seam. Branches on the widget pointer because this class
    // decided where each widget was mounted.
    void onPresentAppRequested(QWidget* widget) override;

protected:
    // Keeps the overlay sized to the full MainContainer: it floats over both
    // the sidebar and the content stack, so it can't sit in the HBoxLayout.
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    bool event(QEvent* event) override;

private slots:
    // Called from QML when the combined visibility of the three overlay
    // dialogs flips. Toggles mouse-event passthrough on the overlay
    // QQuickWidget: transparent when no dialog is open, so the sidebar and
    // content still receive clicks, intercepting when one is.
    void onOverlayActiveChanged(bool active);
    void onSidebarTooltipRequested(const QString& text, qreal y);
    void onAddApplicationDialogRequested(const QVariantMap& metadata);
    // Desktop layout: the shell's own page for a role nothing fills.
    void openSystemApp(const QString& key);
    // Open whatever app fills `role` (ADR-067). false: nothing can.
    bool openRole(const QString& role);
    // The home app's words for the bar (backend homePlaceholder) into TabArea.
    void pullHomePlaceholder();
    // Desktop layout: the dock's app tiles, and its rebuild.
    void launchFromDock(const QString& name);
    void updateDock();
    void openSettingsRepositories();
    // Desktop layout: the Basecamp overlay, the home app over a dimmed
    // desktop (basecamp-experiments#43). Ctrl+K, the bar's Search and the
    // dock's Basecamp toggle it; Esc and a click outside close it.
    void showHomeOverlay();
    void hideHomeOverlay();
    void toggleHomeOverlay();
    // Desktop layout: the Activity sidebar, the same home app in its feed
    // mode on the right edge (basecamp-experiments#44). The bar's Activity
    // button toggles it; Esc and a click outside close it. One home view, so
    // the sidebar and the overlay replace each other.
    void showActivitySidebar();
    void toggleActivitySidebar();
    void homeScrimClicked(int x, int y);
    // The overlay panel's side edges (HomeOverlay.qml): a drag of the right
    // (or left) edge to `x` resizes it, centred; the release keeps the width; a double-click
    // toggles the default and the widest. Kept in kHomeWidthKey.
    void homeEdgeDragged(int x, bool rightEdge);
    void homeEdgeReleased();
    void homeEdgeDoubleClicked();
    // Desktop layout: home asked for an app or a role; it comes up in its
    // window and the overlay closes.
    void desktopLaunchFromHome(const QString& name);
    void desktopRoleFromHome(const QString& role);
    // Basecampium (tabs): the New tab page's two signals.
    void tabOpenSystem(const QString& key);
    void tabLaunchApp(const QString& name);
    void tabOpenRole(const QString& role);
    void loadHomeApp();
    void onNotificationsChanged();
    void onNotifyLayerResized();

private:
    bool applicationsInFront() const;
    void wireSidebar(QObject* sidebarRoot);
    bool offersSection(int section) const;
    void applyAppManagerSearch(const QString& query);
    void invokeOpenApp(const QString& name, const QString& repositoryUrl);
    void setupUi();

    // ── Desktop layout (ShellPrefs::activeLayout() == "desktop") ──
    void setupDesktopUi();
    void positionDock();
    void positionHomeOverlay();
    // Either surface of home: the overlay or the Activity sidebar.
    bool homeOverlayOpen() const;
    void openHomeSurface(bool sidebar);
    QStringList hiddenFromWindowLists() const;
    void onDesktopCloseRequested(const QString& key);
    QWidget* systemAppContent(const QString& key);
    QVariantMap launcherEntry(const QString& name) const;
    void applyGlass(QWidget* widget);

    // ── Basecampium: tabs (built with BASECAMPIUM) ──
    void setupTabsUi();
    void openSystemTab(const QString& key);
    void openAppTab(const QString& module, const QString& label, const QString& icon, QWidget* widget);
    void closeTab(const QString& key);
    void dropPendingNewTab();
    void reopenClosedTab();
    void saveTabs();
    void restoreTabs();
    bool tabsEdgeResize(QObject* watched, QEvent* event);
    bool tabsKey(QKeyEvent* event);
    // Notifications (NOTIFICATIONS-SPEC.md): toasts + list over the tabs.
    void setupNotifications();
    void positionNotifications();

    ShellPrefs* m_prefs = nullptr;
    bool m_desktopLayout = false;
    DesktopArea* m_desktop = nullptr;
    QQuickWidget* m_dockWidget = nullptr;
    QQuickWidget* m_homeScrim = nullptr;         // HomeOverlay.qml, under home's view
    bool m_homeSidebar = false;                  // it is shown as the Activity sidebar
    int m_homeWidth = 0;                         // the overlay's chosen width, 0 = default
    DesktopStatusBar* m_statusBar = nullptr;
    DesktopRoot* m_desktopRoot = nullptr;
    bool m_wentFullScreen = false;

    bool m_tabsLayout = false;
    TabArea* m_tabArea = nullptr;
    QWidget* m_newTabWidget = nullptr;          // New tab: hosts the home role's app
    class QStackedLayout* m_homeStack = nullptr;
    QPointer<QWidget> m_homeWidget;             // the home app's view, when loaded
    QString m_homeApp;                          // the module filling `home`
    QString m_homeQuery;                        // the one bar's text on home
    QObject* homeRoot() const;
    void adoptHomeWidget(QWidget* widget);
    QQuickWidget* m_notifyWidget = nullptr;
    QString m_pendingNewTab;      // the New tab an app was picked from
    QStringList m_closedKeys;     // for Ctrl+Shift+T
    bool m_restoringTabs = false;
    int m_edgeCursor = 0;

    QHBoxLayout* m_mainLayout;

    QQuickWidget* m_sidebarWidget;

    QStackedWidget* m_contentStack;

    // Workspace (QDockWidget-based, replaces the old QMdiArea workspace)
    WorkspaceArea* m_workspaceArea;

    // The `packages` role's page when no installed app can fill it (desktop and
    // tabs layouts; the sidebar layout keeps one in the content stack).
    QPointer<QWidget> m_packagesFallback;
    QPointer<QWidget> m_settingsFallback;
    QPointer<QWidget> m_launcherFallback;

    // Content views (QML for Dashboard, Modules, PackageManager, Settings)
    QQuickWidget* m_contentWidget;

    // Full-window overlay for the dependency-aware dialogs
    // (missing-deps popup, cascade-unload/uninstall confirmations).
    // Lives outside the content stack so it's visible regardless of
    // which stack page is current — previously the dialogs were
    // anchored inside m_contentWidget and never rendered while the
    // Apps/MDI screen was active.
    QQuickWidget* m_overlayWidget;

    // Not owned — Window owns it and it outlives this widget.
    IShellHost* m_host;

    ShortcutBridge* m_shortcutBridge = nullptr;
};

