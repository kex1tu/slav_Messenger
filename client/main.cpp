#include "mainwindow.h"  
#include <QFile>          
#include <QDebug>         
#include <QApplication>   
#include <dataservice.h>

int main(int argc, char *argv[])
{
    // Создаем приложение Qt с переданными аргументами
    QApplication a(argc, argv);

    // Загружаем CSS стили из ресурсов
    QFile styleFile(":/styles.css");
    if (!styleFile.open(QFile::ReadOnly)) {
        // Предупреждение если стили не загрузились
        qWarning() << "Warning: Could not open style file from resources.";
    } else {
        // Применяем стили ко всему приложению
        QString styleSheet = QLatin1String(styleFile.readAll());
        a.setStyleSheet(styleSheet);
        qDebug() << "Style sheet loaded successfully.";
        styleFile.close();
    }

    // Создаем сервис данных (общий для всего приложения)
    DataService dataService;

    // Создаем главное окно, передавая сервис данных
    MainWindow w(&dataService);

    // Показываем главное окно
    w.show();

    // Запускаем главный цикл событий приложения
    return a.exec();
}
