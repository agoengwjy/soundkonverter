
#include "aacreplaygainglobal.h"

#include "soundkonverter_replaygain_aacgain.h"

#include <KDialog>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>


soundkonverter_replaygain_aacgain::soundkonverter_replaygain_aacgain( QObject *parent, const QStringList& args  )
    : ReplayGainPlugin( parent )
{
    binaries["aacgain"] = "";
    
    allCodecs += "m4v";
    allCodecs += "mp3";
    
    KSharedConfig::Ptr conf = KGlobal::config();
    KConfigGroup group;
    
    group = conf->group( "Plugin-"+name() );
    tagMode = group.readEntry( "tagMode", 0 );
}

soundkonverter_replaygain_aacgain::~soundkonverter_replaygain_aacgain()
{}

QString soundkonverter_replaygain_aacgain::name()
{
    return global_plugin_name;
}

QList<ReplayGainPipe> soundkonverter_replaygain_aacgain::codecTable()
{
    QList<ReplayGainPipe> table;
    ReplayGainPipe newPipe;

    newPipe.codecName = "m4a";
    newPipe.rating = 100;
    newPipe.enabled = ( binaries["aacgain"] != "" );
    newPipe.problemInfo = standardMessage( "replygain_codec,backend", "m4a", "aacgain" ) + "\n" + standardMessage( "install_patented_backend", "aacgain" );
    table.append( newPipe );

    newPipe.codecName = "mp3";
    newPipe.rating = 95;
    newPipe.enabled = ( binaries["aacgain"] != "" );
    newPipe.problemInfo = standardMessage( "replygain_codec,backend", "mp3", "aacgain" ) + "\n" + standardMessage( "install_patented_backend", "aacgain" );
    table.append( newPipe );

    return table;
}

bool soundkonverter_replaygain_aacgain::isConfigSupported( ActionType action, const QString& codecName )
{
    return true;
}

void soundkonverter_replaygain_aacgain::showConfigDialog( ActionType action, const QString& codecName, QWidget *parent )
{    if( !configDialog.data() )
    {
        configDialog = new KDialog( parent );
        configDialog.data()->setCaption( i18n("Configure %1").arg(global_plugin_name)  );
        configDialog.data()->setButtons( KDialog::Ok | KDialog::Cancel /*| KDialog::Apply*/ );

        QWidget *configDialogWidget = new QWidget( configDialog.data() );
        QHBoxLayout *configDialogBox = new QHBoxLayout( configDialogWidget );
        QLabel *configDialogTagLabel = new QLabel( i18n("Use tag format:"), configDialogWidget );
        configDialogBox->addWidget( configDialogTagLabel );
        configDialogTagLabelComboBox = new QComboBox( configDialogWidget );
        configDialogTagLabelComboBox->addItem( "APE" );
        configDialogTagLabelComboBox->addItem( "ID3v2" );
        configDialogBox->addWidget( configDialogTagLabelComboBox );

        configDialog.data()->setMainWidget( configDialogWidget );
        connect( configDialog.data(), SIGNAL( okClicked() ), this, SLOT( configDialogSave() ) );
        //connect( configDialog.data(), SIGNAL( applyClicked() ), this, SLOT( configDialogSave() ) );
        //connect( configDialogTagLabelComboBox, SIGNAL( changed( bool ) ), this, SLOT( enableButtonApply( bool ) ) );

        configDialog.data()->enableButtonApply( false );
    }
    configDialogTagLabelComboBox->setCurrentIndex( tagMode );
    configDialog.data()->show();
}

void soundkonverter_replaygain_aacgain::configDialogSave()
{
    if( configDialog.data() )
    {
        tagMode = configDialogTagLabelComboBox->currentIndex();

        KSharedConfig::Ptr conf = KGlobal::config();
        KConfigGroup group;
        
        group = conf->group( "Plugin-"+name() );
        group.writeEntry( "tagMode", tagMode );
    }
}

bool soundkonverter_replaygain_aacgain::hasInfo()
{
    return false;
}

void soundkonverter_replaygain_aacgain::showInfo( QWidget *parent )
{}

int soundkonverter_replaygain_aacgain::apply( const KUrl::List& fileList, ReplayGainPlugin::ApplyMode mode )
{
    if( fileList.count() <= 0 )
        return -1;

    ReplayGainPluginItem *newItem = new ReplayGainPluginItem( this );
    newItem->id = lastId++;
    newItem->process = new KProcess( newItem );
    newItem->process->setOutputChannelMode( KProcess::MergedChannels );
    connect( newItem->process, SIGNAL(readyRead()), this, SLOT(processOutput()) );

//     newItem->mode = mode;

    (*newItem->process) << binaries["aacgain"];
    (*newItem->process) << "-k";
    if( mode == ReplayGainPlugin::Add )
    {
        (*newItem->process) << "-a";
        connect( newItem->process, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(processExit(int,QProcess::ExitStatus)) );
    }
    else if( mode == ReplayGainPlugin::Force )
    {
        (*newItem->process) << "-s";
        (*newItem->process) << "r";
        connect( newItem->process, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(processExit(int,QProcess::ExitStatus)) );
    }
    else
    {
        (*newItem->process) << "-u";
        connect( newItem->process, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(undoProcessExit(int,QProcess::ExitStatus)) );
        undoFileList = fileList;
    }
    if( tagMode == 0 )
    {
        (*newItem->process) << "-s";
        (*newItem->process) << "a";
    }
    else
    {
        (*newItem->process) << "-s";
        (*newItem->process) << "i";
    }
    for( int i = 0; i < fileList.count(); i++ )
    {
        (*newItem->process) << fileList.at(i).toLocalFile();
    }
    newItem->process->start();

    backendItems.append( newItem );
    return newItem->id;
}

void soundkonverter_replaygain_aacgain::undoProcessExit( int exitCode, QProcess::ExitStatus /*exitStatus*/ )
{
    if( undoFileList.count() <= 0 )
        return;
    
    ReplayGainPluginItem *item = 0;

    for( int i=0; i<backendItems.size(); i++ )
    {
        if( backendItems.at(i)->process == QObject::sender() )
        {
            item = (ReplayGainPluginItem*)backendItems.at(i);
            break;
        }
    }

    if( !item )
        return;
    
    if( item->process )
        item->process->deleteLater();
    
    item->process = new KProcess( item );
    item->process->setOutputChannelMode( KProcess::MergedChannels );
    connect( item->process, SIGNAL(readyRead()), this, SLOT(processOutput()) );
    connect( item->process, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(processExit(int,QProcess::ExitStatus)) );

    (*item->process) << binaries["aacgain"];
    (*item->process) << "-s";
    (*item->process) << "d";
    if( tagMode == 0 )
    {
        (*item->process) << "-s";
        (*item->process) << "a";
    }
    else
    {
        (*item->process) << "-s";
        (*item->process) << "i";
    }
    for( int i=0; i<undoFileList.count(); i++ )
    {
        (*item->process) << undoFileList.at(i).toLocalFile();
    }
    item->process->start();
}

float soundkonverter_replaygain_aacgain::parseOutput( const QString& output )
{
    //  9% of 45218064 bytes analyzed
    // [1/10] 32% of 13066690 bytes analyzed
    
    float progress = -1.0f;

    QRegExp reg1("\\[(\\d+)/(\\d+)\\] (\\d+)%");
    QRegExp reg2("(\\d+)%");
    if( output.contains(reg1) )
    {
        float fraction = 1.0f/reg1.cap(2).toInt();
        progress = 100*(reg1.cap(1).toInt()-1)*fraction + reg1.cap(3).toInt()*fraction;
    }
    else if( output.contains(reg2) )
    {
        progress = reg2.cap(1).toInt();
    }
    
    return progress;
/*
    if( output == "" || !output.contains("%") ) return -1.0f;

    QString data = output;
    int space = data.indexOf(" ") + 1;
    int percent = data.indexOf("%");
    data = data.mid( space, percent-space );
    return data.toFloat();*/
}

#include "soundkonverter_replaygain_aacgain.moc"


