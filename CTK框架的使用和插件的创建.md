# CTK框架的使用和插件的创建

**以下项目结构的形式基本会用到我们实际的导航系统的开发中，这个基本的框架结构是需要了解的**

**同时下面的关于CTK框架的搭建和插件的创建都是一些基本的范式，很多代码语句时不需要我们去修改的，理解他的作用，具体的功能实现，后面会通过实现类来实现**

#### 1、刚开始创建时在`QtCreator`中选择*新建-其他项目-子目录项目*，新建项目名称为`SampleCTK`

![image-20250616211124874](C:\Users\cheng\AppData\Roaming\Typora\typora-user-images\image-20250616211124874.png)

#### 2、将CTK文件夹复制到项目文件夹下

CTK文件夹包含install文件夹（CTK编译后下载的），CTK-master文件夹（CTK的源码），build文件夹（CTK的构建文件夹），不拖进来就会老出现找不到某某文件的报错，为了避免麻烦，直接暴力拖拽

![image-20250616212407595](C:\Users\cheng\AppData\Roaming\Typora\typora-user-images\image-20250616212407595.png)

#### 3、在CTK文件夹中创建`CTK.pri`文件

直接创建一手.txt文本文件,编辑文本文件，内容如下

```
# CTK 安装路径
CTK_INSTALL_PATH = $$PWD/../CTK/CTK_install

# CTK 插件相关库所在路径（例如：CTKCore.lib、CTKPluginFramework.lib）
CTK_LIB_PATH = $$CTK_INSTALL_PATH/lib/ctk-0.1

# CTK 插件相关头文件所在路径（例如：ctkPluginFramework.h）
CTK_INCLUDE_PATH = $$CTK_INSTALL_PATH/include/ctk-0.1

# CTK 插件相关头文件所在路径（主要因为用到了 service 相关东西）
CTK_INCLUDE_FRAMEWORK_PATH = $$PWD/../CTK/CTK-master/Libs/PluginFramework

LIBS += -L$$CTK_LIB_PATH -lCTKCore -lCTKPluginFramework

INCLUDEPATH += $$CTK_INCLUDE_PATH \
               $$CTK_INCLUDE_FRAMEWORK_PATH

```

再以个别更改拓展名的形式保存为`CTK.pri`文件

#### 4、创建一个控制台项目`consle项目`命名为APP，对APP.pro文件进行编辑

这些语句基本也是范式，主要include语句，需要准确将.pri文件加载到项目中来。

```
QT = core

CONFIG += c++17 cmdline

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        main.cpp

include($$PWD/../CTK/CTK.pri)

# 添加CTK插件框架相关配置
DEFINES += CTK_PLUGIN_FRAMEWORK
DEFINES += CTK_PLUGIN_FRAMEWORK_EXPORT

# 设置输出目录
DESTDIR = $$OUT_PWD/../bin

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

```

此时你会发现你的项目目录很快啊一下子弹出一个文件，上图！

![image-20250616212910855](C:\Users\cheng\AppData\Roaming\Typora\typora-user-images\image-20250616212910855.png)

#### 5、创建插件：右键项目（`SampleCTK`）选择*新建子项目-其他项目-Empty qmake Project*，项目名称为`HelloCTK`，pro文件中添加代码：

```
QT += core
QT -= gui

TEMPLATE = lib
CONFIG += plugin
TARGET = HelloCTK
DESTDIR = $$OUT_PWD/../bin/plugins

include($$PWD/../CTK.pri)
```

***生成的插件名(TARGET)不要有下划线，因为CTK会默认将插件名中的下划线替换成点号，最后后就导致找不到插件。***

**单个插件最基本的格式要求分成Activator，`qrc`文件，以及MANIFEST.MF,后面还会有实现文件（impl），service文件**

#### 6、项目（插件项目`HelloCTK`）中添加C++类`HelloActivator`，回生成对应的`.h`和.`cpp`文件

CTK插件的接口处理，CTK框架由一个一个可分离的插件组成，框架对插件识别有一定要求，Activator注册器，每个插件都有自己的注册器Activator，单个插件最基本的格式要求分成Activator，qrc文件，以及MANIFEST.MF

*`hello_activator.h`*

```cpp
#ifndef HELLO_ACTIVATOR_H
#define HELLO_ACTIVATOR_H

#include <QObject>
#include "ctkPluginActivator.h"

class HelloActivator : public QObject, public ctkPluginActivator
{
    Q_OBJECT
    Q_INTERFACES(ctkPluginActivator)
    Q_PLUGIN_METADATA(IID "HelloCTK")
    //向Qt的插件框架声明，希望将xxx插件放入到框架中。

public:
    void start(ctkPluginContext* context) override;
    void stop(ctkPluginContext* context) override;

};

#endif // HELLO_ACTIVATOR_H

```

*`hello_activator.cpp`*

```cpp
#include "hello_activator.h"
#include <QDebug>

void HelloActivator::start(ctkPluginContext* context)
{
    qDebug() << "HelloCTK start";
}

void HelloActivator::stop(ctkPluginContext* context)
{
    qDebug() << "HelloCTK stop";
    Q_UNUSED(context)
    //Q_UNUSED,如果一个函数的有些参数没有用到、某些变量只声明不使用，但是又不想编译器、编辑器报警报，其他没有什么实际性作用
}
```

activator是标准的Qt插件类，它实现`ctkPluginActivator`的start、stop函数并对外提供接口。我这里是Qt5的版本，所以使用Q_PLUGIN_METADATA申明插件

#### 7、进入项目目录下，在插件项目（`HelloCTK`）目录下创建MENIFEST.MF文件

与之前`.pri`文件同样的创建方式，内容如下：

```
Plugin-SymbolicName:HelloCTK
Plugin-Version:1.0.0
Plugin-Number:100 #元数据
```

#### 8、创建插件的资源文件

右键`HelloCTK`插件项目，选择添加新文件，选择Qt Resource File

![image-20250616214837148](C:\Users\cheng\AppData\Roaming\Typora\typora-user-images\image-20250616214837148.png)

文件名为`qresource`

此时会出现一个页面，前缀为    插件加载后会寻找*同名前缀/META-INF*，所以前缀格式固定，为   插件名字*/META-INF*    这里命名为`/HelloCTK/META-INF`

选择添加文件，将刚才创建的MENIFEST.MF文件添加进来

#### 9、修改.pro文件，往.pro文件中添加语句

```
file.path = $$DESTDIR
file.files = MANIFEST.MF
INSTALLS += file

```

整体.pro文件为

```
QT += core
QT -= gui

TEMPLATE = lib
CONFIG += plugin
TARGET = HelloCTK
DESTDIR = $$OUT_PWD/../bin/plugins

include($$PWD/../CTK/CTK.pri)

# 添加插件框架相关配置
DEFINES += CTK_PLUGIN_FRAMEWORK
DEFINES += CTK_PLUGIN_FRAMEWORK_EXPORT

HEADERS += \
    hello_activator.h 

SOURCES += \
    hello_activator.cpp 

RESOURCES += \
    qresource.qrc

file.path = $$DESTDIR
file.files = MANIFEST.MF
INSTALLS += file

```

#### 10、编辑App项目的main文件

```cpp
#include <QCoreApplication>
#include <QDirIterator>
#include <QtDebug>
#include <ctkPluginFrameworkFactory.h>
#include <ctkPluginFramework.h>
#include <ctkPluginException.h>
#include <ctkPluginContext.h>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("SampleCTK");//给框架创建名称，Linux下没有会报错
    ctkPluginFrameworkFactory frameWorkFactory;
    QSharedPointer<ctkPluginFramework> framework = frameWorkFactory.getFramework();
    try {
        // 初始化并启动插件框架
        framework->init();
        framework->start();
        qDebug() << "CTK Plugin Framework start ...";
    } catch (const ctkPluginException &e) {
        qDebug() << "Failed to initialize the plugin framework: " << e.what();
        qDebug() << e.message() << e.getType();
        return -1;
    }

    // 获取插件上下文
    ctkPluginContext* context = framework->getPluginContext();

    // 获取插件所在位置
    QString path = QCoreApplication::applicationDirPath() + "/plugins";
    qDebug() << path;

    // 遍历路径下的所有插件
    QDirIterator itPlugin(path, QStringList() << "*.dll" << "*.so", QDir::Files);
    while (itPlugin.hasNext()) {
        QString strPlugin = itPlugin.next();
        try {
            // 安装插件
            QSharedPointer<ctkPlugin> plugin = context->installPlugin(QUrl::fromLocalFile(strPlugin));
            // 启动插件
            plugin->start(ctkPlugin::START_TRANSIENT);
            qDebug() << "Plugin start ...";
        } catch (const ctkPluginException &e) {
            qDebug() << "Failed to install plugin" << e.what();
            return -1;
        }
    }
    return app.exec();
}

```

最后Demo的实现效果，`helloCTK`时候后面插件通信要提到的，这里给出的效果是实现通信后的效果，按照文档的进行，是不会出现最后一段输出的

![image-20250616220307552](C:\Users\cheng\AppData\Roaming\Typora\typora-user-images\image-20250616220307552.png)