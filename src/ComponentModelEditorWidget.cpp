#include "ComponentModelEditorWidget.hpp"
#include "XRockGUI.hpp"
#include "ConfigureDialog.hpp"
#include "ConfigMapHelper.hpp"

#include <QVBoxLayout>
#include <QLabel>
#include <QGridLayout>
#include <QPushButton>
#include <QRegExp>
#include <QTextEdit>
#include <QFileDialog>
#include <QDateTime>
#include <QMessageBox>
#include <QDebug>
#include <bagel_gui/BagelGui.hpp>
#include <bagel_gui/BagelModel.hpp>
#include <mars/utils/misc.h>
#include <QDesktopServices>
#include <xtypes/ComponentModel.hpp>

using namespace configmaps;

namespace xrock_gui_model
{

    ComponentModelEditorWidget::ComponentModelEditorWidget(mars::cfg_manager::CFGManagerInterface *cfg,
                             bagel_gui::BagelGui *bagelGui, XRockGUI *xrockGui,
                             QWidget *parent) : mars::main_gui::BaseWidget(parent, cfg, "ComponentModelEditorWidget"), bagelGui(bagelGui),
                                                xrockGui(xrockGui)
    {
        try
        {
            QGridLayout *layout = new QGridLayout();
            QVBoxLayout *vLayout = new QVBoxLayout();
            size_t i = 0;
            // 20221107 MS: Why does this widget set a model path?
            auto cm = std::make_shared<ComponentModel>();
            const nl::json props = cm->get_properties();
            for (auto it = props.begin(); it != props.end(); ++it)
            {
                if (it->is_null())
                    continue; // skip for now..

                const QString key = QString::fromStdString(it.key());
                const QString value = QString::fromStdString(it.value());
                QLabel *label = new QLabel(key);
                layout->addWidget(label, i, 0);
                const auto allowed_values = cm->get_allowed_property_values(key.toStdString());
                if (allowed_values.size() > 0)
                {
                    // if property has some allowed values, its a combobox
                    QComboBox *combobox = new QComboBox();
                    for (const auto &allowed : allowed_values)
                        combobox->addItem(QString::fromStdString(allowed));
                    layout->addWidget(combobox, i++, 1);
                    connect(combobox, SIGNAL(textChanged(const QString &)), this, SLOT(updateModel()));
                    widgets[label] = combobox;
                }
                else
                {
                    // if property has no allowed values, its a qlineedit
                    QLineEdit *linedit = new QLineEdit();
                    layout->addWidget(linedit, i++, 1);
                    connect(linedit, SIGNAL(textChanged(const QString &)), this, SLOT(updateModel()));
                    widgets[label] = linedit;
                }
            }

            // 20221107 MS: What are annotations?
            //l = new QLabel("annotations");
            //layout->addWidget(l, i, 0);
            //annotations = new QTextEdit();
            //layout->addWidget(annotations, i++, 1);
            //dataStatusLabel = new QLabel();
            //dataStatusLabel->setText("valid Yaml syntax");
            //dataStatusLabel->setAlignment(Qt::AlignCenter);
            //if (annotations)
            //{
            //    dataStatusLabel->setStyleSheet("QLabel { background-color: #128260; color: white; }");
            //    layout->addWidget(dataStatusLabel, i++, 1);
            //    connect(annotations, SIGNAL(textChanged()), this, SLOT(validateYamlSyntax()));
            //}

            QLabel *l = new QLabel("interfaces");
            layout->addWidget(l, i, 0);
            interfaces = new QTextEdit();
            interfaces->setReadOnly(true);
            connect(interfaces, SIGNAL(textChanged()), this, SLOT(updateModel()));
            layout->addWidget(interfaces, i++, 1);
            vLayout->addLayout(layout);

            vLayout->addStretch();

            QGridLayout *gridLayout = new QGridLayout();
            l = new QLabel("Layout:");
            gridLayout->addWidget(l, 0, 0);
            i = 0;
            for (const auto allowed : cm->get_allowed_property_values("domain"))
            {
                QCheckBox *check = new QCheckBox(QString::fromStdString(allowed));
                check->setChecked(true);
                connect(check, SIGNAL(stateChanged(int)), this, SLOT(setViewFilter(int)));
                gridLayout->addWidget(check, i / 2, i % 2 + 1);
                layoutCheckBoxes[allowed] = check;
                i++;
            }
            vLayout->addLayout(gridLayout);
            layouts = new QListWidget();
            vLayout->addWidget(layouts);
            connect(layouts, SIGNAL(clicked(const QModelIndex &)),
                    this, SLOT(layoutsClicked(const QModelIndex &)));
            layouts->addItem("overview");
            QHBoxLayout *hLayout = new QHBoxLayout();
            layoutName = new QLineEdit("new layout");
            hLayout->addWidget(layoutName);
            QPushButton *b = new QPushButton("add/remove");
            connect(b, SIGNAL(clicked()), this, SLOT(addRemoveLayout()));
            hLayout->addWidget(b);
            vLayout->addLayout(hLayout);


            // 20221107 MS: Removed buttons in favor of the XRock Toolbar
            setLayout(vLayout);
            currentLayout = "";
            this->clear();

            // 20221107 MS: What is the XRock config filter?
            //xrockConfigFilter.push_back("activity");
            //xrockConfigFilter.push_back("state");
            //xrockConfigFilter.push_back("config_names");
            //xrockConfigFilter.push_back("parentName");

        }
        catch (const std::exception &e)
        {
            std::stringstream ss;
            ss << "Exception thrown: " << e.what() << "\tAt " << __FILE__ << ':' << __LINE__ << '\n'
               << "\tAt " << __PRETTY_FUNCTION__ << '\n';
            QMessageBox::warning(nullptr, "Warning", QString::fromStdString(ss.str()), QMessageBox::Ok);
        }
    }

    ComponentModelEditorWidget::~ComponentModelEditorWidget(void)
    {
        // Cleanup widgets
        for(auto& [label, widget] : widgets)
        {
            delete label;
            delete widget;
        }
    }

    void ComponentModelEditorWidget::deinit(void)
    {
        // 20221107 MS: Why does this widget set a model path?
        //cfg->setPropertyValue("XRockGUI", "modelPath", "value", modelPath);
    }

    void ComponentModelEditorWidget::update_widgets(configmaps::ConfigMap& info)
    {           
        // Lets iterate over model's properties
        nl::json info_data = nl::json::parse(info.toJsonString());
        auto is_property_key = [&](const std::string& key) -> bool
        {
           static auto cm = std::make_shared<ComponentModel>();
            const nl::json props = cm->get_properties();
            for (auto it = props.begin(); it != props.end(); ++it)
                if(key == it.key())
                    return true;
           return false;
        };
       for (auto it = info_data.begin(); it != info_data.end(); ++it)
         {   
            std::string key = it.key();
            nl::json value = it.value();
            if(is_property_key(key))
              this->update_prop_widget(key,  value);

         }
   
        for (auto it = info_data["versions"][0].begin(); it != info_data["versions"][0].end(); ++it)
        {
            //std::cout << it.key() << " sec" << it.value() << std::endl;
            std::string key = it.key();
            nl::json value = it.value();

            if(key == "name")
                key = "version"; // maybe just rename it so when we pass it to update_prop_widget it will update the version widget
                

            if(not is_property_key(key) or value.is_object()) continue; // skip interfaces .. non-properties
              this->update_prop_widget(key,  value);
   

        }
        if(info["versions"][0].hasKey("interfaces"))
        {
            std::cout << "interfaces lolipop: "<< info["versions"][0]["interfaces"].toYamlString().c_str() << std::endl;
            std::string inter = info["versions"][0]["interfaces"].toYamlString().c_str();
            interfaces->setText(QString::fromStdString(inter));
              //this->update_prop_widget("interfaces", interfaces);
        }
             /*
        ConfigMap::iterator it = info.beginMap();
        while(it != info.endMap()) {
            std::cout << it->first << " sec" << it->second.toString() << std::endl;

      if(it->second.isAtom()) {
          std::string value = trim(it->second.toString());
          if(value.empty()) {
            item.erase(it);
            it = item.beginMap();
          }
          else {
            ++it;
          }
        }
        else if(it->second.isMap() || it->second.isVector()) {
          // todo: handle empty map
          trimMap(it->second);
          if(it->second.size() == 0) {
            item.erase(it);
            it = item.beginMap();
          }
          else {
            ++it;
          }
        }
      }
        */
        //for ( auto & it : info["versions"][0])
            //std::cout << it.first << " sec" << it[it.first].toString() << std::endl;
        // for (auto it = info["versions"][0].begin(); it != info["versions"][0].end(); ++it)
        // {
        //     std::string key = (ConfigMap)it.first;
        //     ConfigMap value = (ConfigMap)it.second;

        //     this->update_prop_widget(key,value.toString());
        // }

            //if(it->first == "versions" )
              //std::cout << info["versions"]["name"].toJsonString()<< std::endl;
    
    }   

    void ComponentModelEditorWidget::currentModelChanged(bagel_gui::ModelInterface *model)
    {
        ComponentModelInterface* newModel = dynamic_cast<ComponentModelInterface *>(model);
        if (!newModel) return;
        // TODO: Update all fields with the info given by the map. We should NOT trigger textChanged() though!
        auto info = newModel->getModelInfo();
        this->update_widgets(info);
 currentModel = newModel;
   // maybe we try the properties method now ? 
// this method needs manual handling 
#if 0
      
    #endif
    //     for ( const auto &it : info) {
    //     std::cout << it.first<< "" << info[it.first]  << "\n";
    // }
        //std::cout << "hi there " << info.toJsonString() << std::endl;
          //  this->update_prop_widget("name", info["name"]);
        //this->update_prop_widget("type", info["type"]);
        // Set the newModel to be the current model
        // which will also allow updates to the model via updateModel()
        //currentModel = newModel;
    }

    void ComponentModelEditorWidget::update_prop_widget(const std::string &prop_name, const std::string &value)
    {
        for (auto &[label, widget] : widgets)
        {
            if (label->text().toStdString() == prop_name)
            {
                if(QLineEdit * le = dynamic_cast<QLineEdit *>(widget))
                {
                    le->setText(QString::fromStdString(value));
                    break;
                }
                else if(QComboBox * cb = dynamic_cast<QComboBox *>(widget))
                {
                  
                    cb->setCurrentIndex(cb->findData(QString::fromStdString(value), Qt::DisplayRole)); // <- refers to the item text
                    break;
                }
            }
        }
    }
    std::string ComponentModelEditorWidget::get_prop_widget_text(const std::string &prop_name)
    {
        for (auto &[label, widget] : widgets)
        {
            if (label->text().toStdString() == prop_name)
            {
                if (QComboBox *cb = dynamic_cast<QComboBox *>(widget))
                    return cb->currentText().toStdString();

                return dynamic_cast<QLineEdit *>(widget)->text().toStdString();
            }
        }
        throw std::runtime_error("no prop found with name " + prop_name);
    }
    void ComponentModelEditorWidget::updateModel()
    {
        if (!currentModel) return;
        ConfigMap updatedMap(currentModel->getModelInfo());
        updatedMap["name"] = get_prop_widget_text("name");
        

        // TODO: Read out the other fields and update the model properties of the currentModel
        currentModel->setModelInfo(updatedMap);
    }

    void ComponentModelEditorWidget::setViewFilter(int v)
    {
        for (const auto& [label, checkbox] : layoutCheckBoxes)
        {
            bagelGui->setViewFilter(label, checkbox->isChecked());
        }
    }

    void ComponentModelEditorWidget::layoutsClicked(const QModelIndex &index)
    {

        try
        {
            QVariant v = layouts->model()->data(index, 0);
            if (v.isValid())
            {
                std::string layout = v.toString().toStdString();
                updateCurrentLayout();
                currentLayout = layout;
                layout += ".yml";
                layoutName->setText(currentLayout.c_str());

                bagelGui->loadLayout(layout);
                bagelGui->applyLayout(layoutMap[currentLayout]);
            }
        }
        catch (const std::exception &e)
        {
            std::stringstream ss;
            ss << "Exception thrown: " << e.what() << "\tAt " << __FILE__ << ':' << __LINE__ << '\n'
               << "\tAt " << __PRETTY_FUNCTION__ << '\n';
            QMessageBox::warning(nullptr, "Warning", QString::fromStdString(ss.str()), QMessageBox::Ok);
        }
    }

    void ComponentModelEditorWidget::addRemoveLayout()
    {
        try
        {
            std::string name = layoutName->text().toStdString();
            for (int i = 0; i < layouts->count(); ++i)
            {
                QVariant v = layouts->item(i)->data(0);
                if (v.isValid())
                {
                    std::string layout = v.toString().toStdString();
                    if (layout == name)
                    {
                        QListWidgetItem *item = layouts->item(i);
                        delete item;
                        layoutMap.erase(name);
                        if (layouts->count() > 0)
                        {
                            layouts->setCurrentItem(layouts->item(0));
                            currentLayout = layouts->item(0)->data(0).toString().toStdString();
                            layoutName->setText(currentLayout.c_str());
                            bagelGui->applyLayout(layoutMap[currentLayout]);
                        }
                        else
                        {
                            currentLayout = "";
                            layouts->setCurrentItem(0);
                        }
                        updateModel();
                        return;
                    }
                }
            }
            updateCurrentLayout();
            ConfigMap layout;
            layouts->addItem(name.c_str());
            layouts->setCurrentItem(layouts->item(layouts->count() - 1));
            currentLayout = name;
            updateCurrentLayout();
            updateModel();
        }

        catch (const std::exception &e)
        {
            std::stringstream ss;
            ss << "Exception thrown: " << e.what() << "\tAt " << __FILE__ << ':' << __LINE__ << '\n'
               << "\tAt " << __PRETTY_FUNCTION__ << '\n';
            QMessageBox::warning(nullptr, "Warning", QString::fromStdString(ss.str()), QMessageBox::Ok);
        }
    }

    void ComponentModelEditorWidget::updateCurrentLayout()
    {
        if (!currentLayout.empty())
        {
            ConfigMap layout = bagelGui->getLayout();
            layoutMap[currentLayout] = layout;
        }
    }

    void ComponentModelEditorWidget::clear()
    {
        // NOTE: Before clearing the fields, we have to set currentModel to null to prevent updateModel() to be triggered
        currentModel = nullptr;
        for (auto &[label, widget] : widgets)
        {
            if (QLineEdit *field = dynamic_cast<QLineEdit *>(widget))
            {
                field->clear();
            }
        }
        // annotations->clear();
        interfaces->clear();
        layouts->clear();
        layoutMap = ConfigMap();
    }


    void ComponentModelEditorWidget::openUrl(const QUrl &link)
    {
        QDesktopServices::openUrl(link);
    }

    void ComponentModelEditorWidget::validateYamlSyntax()
    {
        const std::string data_text = annotations->toPlainText().toStdString();
        if (data_text.empty())
            return;
        try
        {
            ConfigMap tmpMap = ConfigMap::fromYamlString(data_text);
            dataStatusLabel->setText("valid Yaml syntax");
            dataStatusLabel->setStyleSheet("QLabel { background-color: #128260; color: white;}");
        }
        catch (...)
        {
            dataStatusLabel->setText("invalid Yaml syntax");
            dataStatusLabel->setStyleSheet("QLabel { background-color: red; color: white;}");
        }
    }

} // end of namespace xrock_gui_model
