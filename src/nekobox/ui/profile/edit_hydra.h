

#pragma once
#include <QWidget>
#include "profile_editor.h"
#include "ui_edit_hydra.h"
QT_BEGIN_NAMESPACE

namespace Ui {
    class EditHydra;
}
QT_END_NAMESPACE
class EditHydra : public QWidget, public ProfileEditor {
    Q_OBJECT

public:
    explicit EditHydra(QWidget *parent = nullptr);

    ~EditHydra() override;

    void onStart(std::shared_ptr<Configs::ProxyEntity> _ent) override;

    bool onEnd() override;

private:
    Ui::EditHydra *ui;
    std::shared_ptr<Configs::ProxyEntity> ent;
};
