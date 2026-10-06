#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <vector>
#include "ipv6/ipv6_manager.h"

namespace ProxyBridge {

class SourcesPage : public QWidget {
    Q_OBJECT

public:
    explicit SourcesPage(QWidget* parent = nullptr);
    ~SourcesPage() override;

    void refreshSources();
    void refreshDetection();

private slots:
    void onAdd();
    void onApply();
    void onRemove();
    void onSourceSelected(int index);

private:
    void setupUi();
    QWidget* createSourceList();
    QWidget* createEditor();

    QListWidget* m_sourceList{nullptr};
    QPushButton* m_refreshButton{nullptr};
    std::vector<IPv6Subnet> m_detectedSubnets;
    QLineEdit* m_nameEdit{nullptr};
    QComboBox* m_modeCombo{nullptr};
    QComboBox* m_interfaceCombo{nullptr};
    QLineEdit* m_prefixEdit{nullptr};
    QSpinBox* m_prefixLenSpin{nullptr};
    QSpinBox* m_slotsSpin{nullptr};
    QCheckBox* m_enabledCheck{nullptr};
    QPushButton* m_addButton{nullptr};
    QPushButton* m_applyButton{nullptr};
    QPushButton* m_removeButton{nullptr};

    int m_selectedSourceId{-1};
};

} // namespace ProxyBridge
