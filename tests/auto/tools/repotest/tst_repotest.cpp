/**************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
**************************************************************************/
#include "../../installer/shared/verifyinstaller.h"

#include <repositorygen.h>
#include <repositorygen.cpp>
#include <init.h>

#include <QFile>
#include <QTest>
#include <QRegularExpression>

class tst_repotest : public QObject
{
    Q_OBJECT
private:
    void generateRepo(bool createSplitMetadata, bool createUnifiedMetadata, bool updateNewComponents,
                      QStringList packagesUpdatedWithSha = QStringList())
    {
        QStringList filteredPackages;

        QInstallerTools::PackageInfoVector m_packages = QInstallerTools::collectPackages(m_repoInfo,
            &filteredPackages, QInstallerTools::Exclude, updateNewComponents, packagesUpdatedWithSha);

        if (updateNewComponents) { //Verify that component B exists as that is not updated
            if (createSplitMetadata) {
                VerifyInstaller::verifyFileExistence(m_repoInfo.repositoryDir + "/B", QStringList() << "1.0.0content.7z"
                                                    << "1.0.0content.7z.sha1" << "1.0.0meta.7z");
            } else {
                VerifyInstaller::verifyFileExistence(m_repoInfo.repositoryDir + "/B", QStringList() << "1.0.0content.7z"
                                                    << "1.0.0content.7z.sha1");
            }
        } else {
            QDir dir(m_repoInfo.repositoryDir + "/B");
            QVERIFY(!dir.exists());
        }
        QTemporaryDir tmp;
        tmp.setAutoRemove(false);
        const QString tmpMetaDir = tmp.path();
        QInstallerTools::createRepository(m_repoInfo, &m_packages, tmpMetaDir, createSplitMetadata,
                                          createUnifiedMetadata);
        QInstaller::removeDirectory(tmpMetaDir, true);
    }

    void clearData()
    {
        m_repoInfo.packages.clear();
        m_repoInfo.repositoryPackages.clear();
        m_packages.clear();
    }

    void initRepoUpdate()
    {
        clearData();
        m_repoInfo.packages << ":///packages_update";
    }

    void initRepoUpdateFromRepository(const QString &repository)
    {
        clearData();
        m_repoInfo.repositoryPackages << repository;
    }

    void verifyUniteMetadata(const QString &scriptVersion)
    {
        QString fileContent = VerifyInstaller::fileContent(m_repoInfo.repositoryDir + QDir::separator()
                                            + "Updates.xml");
        QRegularExpression re("<MetadataName>(.*)<.MetadataName>");
        QStringList matches = re.match(fileContent).capturedTexts();
        QString existingUniteMeta7z = QInstallerTools::existingUniteMeta7z(m_repoInfo.repositoryDir);
        QCOMPARE(2, matches.count());
        QCOMPARE(existingUniteMeta7z, matches.at(1));
        QFile file(m_repoInfo.repositoryDir + QDir::separator() + matches.at(1));
        QVERIFY(file.open(QIODevice::ReadOnly));

        //We have script<version>.qs for package A in the unite metadata
        QVector<Lib7z::File>::const_iterator fileIt;
        const QVector<Lib7z::File> files = Lib7z::listArchive(&file);
        for (fileIt = files.begin(); fileIt != files.end(); ++fileIt) {
            if (fileIt->isDirectory)
                continue;
            QString fileName = "A/script%1.qs";
            QCOMPARE(qPrintable(fileName.arg(scriptVersion)), fileIt->path);
        }

        VerifyInstaller::verifyFileExistence(m_repoInfo.repositoryDir, QStringList() << "Updates.xml"
                                            << matches.at(1));
        VerifyInstaller::verifyFileContent(m_repoInfo.repositoryDir + QDir::separator() + "Updates.xml",
                                            "SHA1");
        VerifyInstaller::verifyFileContent(m_repoInfo.repositoryDir + QDir::separator() + "Updates.xml",
                                            "MetadataName");
    }

    void verifyComponentRepository(const QString &componentAVersion, bool hasComponentMeta)
    {
        const QString content = "%1content.7z";
        const QString contentSha1 = "%1content.7z.sha1";
        const QString meta = "%1meta.7z";
        QStringList componentA;
        QStringList componentB;
        componentA << qPrintable(content.arg(componentAVersion)) << qPrintable(contentSha1.arg(componentAVersion));
        componentB << "1.0.0content.7z" << "1.0.0content.7z.sha1";
        if (hasComponentMeta) {
            componentA << qPrintable(meta.arg(componentAVersion));
            componentB << "1.0.0meta.7z";
        }
        VerifyInstaller::verifyFileExistence(m_repoInfo.repositoryDir + "/A", componentA);
        VerifyInstaller::verifyFileExistence(m_repoInfo.repositoryDir + "/B", componentB);
    }

    void verifyComponentMetaUpdatesXml()
    {
        VerifyInstaller::verifyFileExistence(m_repoInfo.repositoryDir, QStringList() << "Updates.xml");
        VerifyInstaller::verifyFileHasNoContent(m_repoInfo.repositoryDir + QDir::separator() + "Updates.xml",
                                           "MetadataName");
    }

    void ignoreMessagesForComponentHash(const QStringList &components, bool update)
    {
        foreach (const QString component, components) {
            QString message = "Copying component data for \"%1\"";
            QTest::ignoreMessage(QtDebugMsg, qPrintable(message.arg(component)));
            if (update)
                message = "Compressing files found in data directory: (\":/packages_update/%1/data/%1_update.txt\")";
            else
                message = "Compressing files found in data directory: (\":/packages/%1/data/%1.txt\")";
            QTest::ignoreMessage(QtDebugMsg, qPrintable(message.arg(component)));
            QTest::ignoreMessage(QtDebugMsg, QRegularExpression("Hash is stored in *"));
            QTest::ignoreMessage(QtDebugMsg, QRegularExpression("Creating hash of archive *"));
            QTest::ignoreMessage(QtDebugMsg, QRegularExpression("Generated sha1 hash: *"));
        }
    }

    void ignoreMessagesForCopyRepository(const QString &component, const QString &version, const QString &repository)
    {
        QStringList contentFiles;
        contentFiles << "content.7z" << "content.7z.sha1";
        QString message = "Copying component data for \"%1\"";
        QTest::ignoreMessage(QtDebugMsg, qPrintable(message.arg(component)));
        foreach (const QString &fileName, contentFiles) {
            message = "Copying file from \":///%5/%1/%2%4\" to \"%3/%1/%2%4\"";
            QTest::ignoreMessage(QtDebugMsg, qPrintable(message.arg(component).arg(version)
                .arg(m_repoInfo.repositoryDir).arg(fileName).arg(repository)));
        }
    }

    void ignoreMessageForCollectingRepository(const QString &repository)
    {
        const QString message = "Process repository \"%2\"";
        QTest::ignoreMessage(QtDebugMsg, "Collecting information about available repository packages...");
        QTest::ignoreMessage(QtDebugMsg, qPrintable(message.arg(repository)));
        QTest::ignoreMessage(QtDebugMsg, "Collecting information about available packages...");
        QTest::ignoreMessage(QtDebugMsg, "No available packages found at the specified location.");
        QTest::ignoreMessage(QtDebugMsg, "- it provides the package \"A\"  -  \"2.0.0\"");
    }

    void ignoreMessagesForCopyMetadata(const QString &component, bool hasMeta, bool update)
    {
        QString message;
        if (update)
            message = "Copy meta data for package \"%1\" using \":///packages_update/%1/meta/package.xml\"";
        else
            message = "Copy meta data for package \"%1\" using \":///packages/%1/meta/package.xml\"";
        QTest::ignoreMessage(QtDebugMsg, qPrintable(message.arg(component)));
        QTest::ignoreMessage(QtDebugMsg, QRegularExpression("calculate size of directory *"));
        if (hasMeta) {
            if (update)
                message = "Copying associated \"script\" file \":///packages_update/%1/meta/script2.0.0.qs\"";
            else
                message = "Copying associated \"script\" file \":///packages/%1/meta/script1.0.0.qs\"";
            QTest::ignoreMessage(QtDebugMsg, qPrintable(message.arg(component)));
            QTest::ignoreMessage(QtDebugMsg, "done.");
        }
    }

    void ignoreMessagesForComponentSha(const QStringList &components, bool clearOldChecksum)
    {
        foreach (const QString component, components) {
            const QString message = "Searching sha1sum node for \"%1\"";
            QTest::ignoreMessage(QtDebugMsg, qPrintable(message.arg(component)));
            if (clearOldChecksum)
                QTest::ignoreMessage(QtDebugMsg, QRegularExpression("- clearing the old sha1sum *"));
            QTest::ignoreMessage(QtDebugMsg, QRegularExpression("- writing the sha1sum *"));
        }
    }

    void ignoreMessagesForUniteMeta(bool update)
    {
        QTest::ignoreMessage(QtDebugMsg, "Writing sha1sum node.");
        if (update)
            QTest::ignoreMessage(QtDebugMsg, QRegularExpression("- clearing the old sha1sum *"));
        QTest::ignoreMessage(QtDebugMsg, QRegularExpression("- writing the sha1sum"));
        QTest::ignoreMessage(QtDebugMsg, QRegularExpression("Updating the metadata node with name *"));
    }

    void ignoreMessageForCollectingPackages(const QString &versionA, const QString &versionB)
    {
        QTest::ignoreMessage(QtDebugMsg, "Collecting information about available repository packages...");
        QTest::ignoreMessage(QtDebugMsg, "Collecting information about available packages...");
        QTest::ignoreMessage(QtDebugMsg, "Found subdirectory \"A\"");
        QString message = "- it provides the package \"A\"  -  \"%1\"";
        QTest::ignoreMessage(QtDebugMsg, qPrintable(message.arg(versionA)));
        QTest::ignoreMessage(QtDebugMsg, "Found subdirectory \"B\"");
        message = "- it provides the package \"B\"  -  \"%1\"";
        QTest::ignoreMessage(QtDebugMsg, qPrintable(message.arg(versionB)));
    }

    void ignoreMessageForCollectingPackagesFromRepository(const QString &versionA, const QString &versionB)
    {
        QString message = "- it provides the package \"B\"  -  \"%1\"";
        QTest::ignoreMessage(QtDebugMsg, qPrintable(message.arg(versionB)));
        message = "- it provides the package \"A\"  -  \"%1\"";
        QTest::ignoreMessage(QtDebugMsg, qPrintable(message.arg(versionA)));
        QTest::ignoreMessage(QtDebugMsg, "Collecting information about available packages...");
        QTest::ignoreMessage(QtDebugMsg, "No available packages found at the specified location.");
    }

    void ignoreMessagesForUpdateComponents()
    {
        ignoreMessageForCollectingPackages("2.0.0", "1.0.0");
        ignoreMessagesForComponentSha(QStringList() << "A" << "B", false);
        ignoreMessagesForComponentHash(QStringList() << "A" << "B", true);
        ignoreMessagesForCopyMetadata("A", true, true);
        ignoreMessagesForCopyMetadata("B", false, true);
    }

    void ignoreMessageForUpdateComponent()
    {
        QString message = "Update component \"A\" in \"%1\" .";
        QTest::ignoreMessage(QtDebugMsg, qPrintable(message.arg(m_repoInfo.repositoryDir)));
        message = "Update component \"C\" in \"%1\" .";
        QTest::ignoreMessage(QtDebugMsg, qPrintable(message.arg(m_repoInfo.repositoryDir)));
    }

    void verifyComponentShaUpdate(int shaUpdateComponents)
    {
        QString updatesXmlFile(m_repoInfo.repositoryDir + QDir::separator() + "Updates.xml");
        QFile file(updatesXmlFile);
        QDomDocument dom;

        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(dom.setContent(&file));
        file.close();
        QCOMPARE(dom.elementsByTagName("ContentSha1").count(), shaUpdateComponents);
        VerifyInstaller::verifyFileContent(updatesXmlFile,
            "<ContentSha1>059e5ed8cd3a1fbca08cccfa4075265192603e3f</ContentSha1>");
    }

private slots:
    void init()
    {
        ignoreMessageForCollectingPackages("1.0.0", "1.0.0");

        m_repoInfo.repositoryDir = QInstallerTools::makePathAbsolute(QInstaller::generateTemporaryFileName());
        m_tempDirDeleter.add(m_repoInfo.repositoryDir);

        m_repoInfo.packages << ":///packages";

        ignoreMessagesForComponentHash(QStringList() << "A" << "B", false);
        ignoreMessagesForCopyMetadata("A", true, false); //Only A has metadata
        ignoreMessagesForCopyMetadata("B", false, false);
    }

    void initTestCase()
    {
        Lib7z::initSevenZ();
    }

    void testWithComponentMeta()
    {
        ignoreMessagesForComponentSha(QStringList () << "A" << "B", false);
        generateRepo(true, false, false);

        verifyComponentRepository("1.0.0", true);
        verifyComponentMetaUpdatesXml();
    }

    void testWithComponentAndUniteMeta()
    {
        ignoreMessagesForComponentSha(QStringList() << "A" << "B", false);
        ignoreMessagesForUniteMeta(false);
        generateRepo(true, true, false);

        verifyComponentRepository("1.0.0", true);
        verifyUniteMetadata("1.0.0");
    }

    void testWithUniteMeta()
    {
        ignoreMessagesForUniteMeta(false);
        generateRepo(false, true, false);

        verifyComponentRepository("1.0.0", false);
        verifyUniteMetadata("1.0.0");
    }

    void testWithComponentShaUpdate()
    {
        ignoreMessagesForComponentSha(QStringList () << "A" << "B", false);
        generateRepo(true, false, false, QStringList () << "A");

        verifyComponentRepository("1.0.0", true);
        verifyComponentMetaUpdatesXml();
        verifyComponentShaUpdate(1);
    }

    void testWithTwoComponentsShaUpdate()
    {
        ignoreMessagesForComponentSha(QStringList () << "A" << "B", false);
        generateRepo(true, false, false, QStringList () << "A" << "B");

        verifyComponentRepository("1.0.0", true);
        verifyComponentMetaUpdatesXml();
        verifyComponentShaUpdate(2);
    }

    void testUpdateNewComponents()
    {
        // Create 'base' repository which will be updated
        ignoreMessagesForComponentSha(QStringList() << "A" << "B", false);
        generateRepo(true, false, false);
        verifyComponentRepository("1.0.0", true);

        // Update the 'base' repository
        initRepoUpdate();
        ignoreMessageForCollectingPackages("2.0.0", "1.0.0");
        ignoreMessagesForComponentSha(QStringList() << "A", false); //Only A has update
        ignoreMessagesForComponentHash(QStringList() << "A", true);
        ignoreMessagesForCopyMetadata("A", true, true);
        const QString &message = "Update component \"A\" in \"%1\" .";
        QTest::ignoreMessage(QtDebugMsg, qPrintable(message.arg(m_repoInfo.repositoryDir)));
        generateRepo(true, false, true);
        verifyComponentRepository("2.0.0", true);
        verifyComponentMetaUpdatesXml();
    }

    void testUpdateComponents()
    {
        ignoreMessagesForComponentSha(QStringList() << "A" << "B", false);
        generateRepo(true, false, false);
        verifyComponentRepository("1.0.0",true);

        initRepoUpdate();
        ignoreMessagesForUpdateComponents();
        generateRepo(true, false, false);
        verifyComponentRepository("2.0.0", true);
        verifyComponentMetaUpdatesXml();
    }

    void testUpdateComponentsWithUniteMetadata()
    {
        ignoreMessagesForComponentSha(QStringList() << "A" << "B", false);
        ignoreMessagesForUniteMeta(false);
        generateRepo(true, true, false);
        verifyComponentRepository("1.0.0", true);

        initRepoUpdate();
        ignoreMessagesForUpdateComponents();
        ignoreMessagesForUniteMeta(true);
        generateRepo(true, true, false);
        verifyComponentRepository("2.0.0", true);
        verifyUniteMetadata("2.0.0");
    }

    void testUpdateComponentsWithOnlyUniteMetadata()
    {
        ignoreMessagesForUniteMeta(false);
        generateRepo(false, true, false);
        verifyComponentRepository("1.0.0", false);

        initRepoUpdate();
        ignoreMessageForCollectingPackages("2.0.0", "1.0.0");
        ignoreMessagesForComponentHash(QStringList() << "A" << "B", true);
        ignoreMessagesForCopyMetadata("A", true, true);
        ignoreMessagesForCopyMetadata("B", false, true);
        ignoreMessagesForUniteMeta(true);
        generateRepo(false, true, false);
        verifyComponentRepository("2.0.0", false);
        verifyUniteMetadata("2.0.0");
    }

    void testUpdateComponentsFromRepository()
    {
        ignoreMessagesForComponentSha(QStringList() << "A" << "B", false);
        generateRepo(true, false, false);
        verifyComponentRepository("1.0.0", true);

        initRepoUpdateFromRepository(":///repository_component");
        ignoreMessageForCollectingRepository("repository_component");
        QTest::ignoreMessage(QtDebugMsg, "- it provides the package \"B\"  -  \"1.0.0\"");

        ignoreMessagesForCopyRepository("B", "1.0.0", "repository_component");
        ignoreMessagesForCopyRepository("A", "2.0.0", "repository_component");
        ignoreMessagesForComponentSha(QStringList() << "A" << "B", true);
        generateRepo(true, false, false);
        verifyComponentRepository("2.0.0", true);
        verifyComponentMetaUpdatesXml();
    }

    void testUpdateComponentsFromRepositoryWithUniteMetadata()
    {
        ignoreMessagesForComponentSha(QStringList() << "A" << "B", false);
        ignoreMessagesForUniteMeta(false);
        generateRepo(true, true, false);
        verifyComponentRepository("1.0.0", true);

        initRepoUpdateFromRepository(":///repository_componentAndUnite");
        ignoreMessageForCollectingRepository("repository_componentAndUnite");
        QTest::ignoreMessage(QtDebugMsg, "- it provides the package \"C\"  -  \"1.0.0\"");
        ignoreMessageForUpdateComponent();
        ignoreMessagesForCopyRepository("A", "2.0.0", "repository_componentAndUnite");
        ignoreMessagesForCopyRepository("C", "1.0.0", "repository_componentAndUnite");
        ignoreMessagesForUniteMeta(true);
        ignoreMessagesForComponentSha(QStringList() << "A" << "C", true);

        generateRepo(true, true, true);
        verifyComponentRepository("2.0.0", true);
        VerifyInstaller::verifyFileExistence(m_repoInfo.repositoryDir + "/C", QStringList() << "1.0.0content.7z" << "1.0.0content.7z.sha1" << "1.0.0meta.7z");
        verifyUniteMetadata("2.0.0");
    }

    void testUpdateComponentsFromRepositoryWithOnlyUniteMetadata()
    {
        ignoreMessagesForUniteMeta(false);
        generateRepo(false, true, false);
        verifyComponentRepository("1.0.0", false);

        initRepoUpdateFromRepository(":///repository_unite");
        ignoreMessageForCollectingRepository("repository_unite");
        QTest::ignoreMessage(QtDebugMsg, "- it provides the package \"C\"  -  \"1.0.0\"");
        ignoreMessageForUpdateComponent();
        ignoreMessagesForCopyRepository("A", "2.0.0", "repository_unite");
        ignoreMessagesForCopyRepository("C", "1.0.0", "repository_unite");
        ignoreMessagesForUniteMeta(true);

        generateRepo(false, true, true);
        verifyComponentRepository("2.0.0", false);
        VerifyInstaller::verifyFileExistence(m_repoInfo.repositoryDir + "/C", QStringList() << "1.0.0content.7z" << "1.0.0content.7z.sha1");
        verifyUniteMetadata("2.0.0");
    }

    void cleanup()
    {
        m_tempDirDeleter.releaseAndDeleteAll();
        m_repoInfo.packages.clear();
        m_packages.clear();
        m_repoInfo.repositoryPackages.clear();
    }

private:
    QInstallerTools::RepositoryInfo m_repoInfo;
    QInstallerTools::PackageInfoVector m_packages;
    TempDirDeleter m_tempDirDeleter;
};

QTEST_MAIN(tst_repotest)

#include "tst_repotest.moc"