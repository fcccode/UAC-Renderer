#include "mainwindow.h"
#include <QBitmap>
#include <QImage>
#include <QPixmap>
#include <QFont>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QMouseEvent>
#include <QApplication>
#include "Windows.h"
#include "wincrypt.h"
#include "WinTrust.h"
#include "Shlwapi.h"

#include "certhandler.h"
PushButton::PushButton(QWidget *parent)
    :QPushButton (parent){
    myFont=this->font();
    myFont.setBold(false);
    myFontBld=this->font();
    myFontBld.setBold(true);
}
PushButton::~PushButton(){}
void PushButton::leaveEvent(QEvent *e){
    emit MouseLeave();
    this->setFont(myFont);
}
void PushButton::enterEvent(QEvent *e){
    emit MouseEnter();
    this->setFont(myFontBld);
}
void MainWindow::mouseMoveEvent(QMouseEvent *e){
    if(isDragging)
        setGeometry(e->globalX()-OffsetX,e->globalY()-OffsetY,600,200);
    if(e->buttons().testFlag(Qt::LeftButton)==false)isDragging=false;
}
void MainWindow::mousePressEvent(QMouseEvent *e){
    OffsetX=e->x();
    OffsetY=e->y();
    isDragging=true;
}
void MainWindow::mouseReleaseEvent(QMouseEvent *e){
    if(isDragging)
        setGeometry(e->globalX()-OffsetX,e->globalY()-OffsetY,600,200);
    isDragging=false;
}
MainWindow::MainWindow(int pid,int len, void* addr,bool IsSec,HDESK OldDesk)
    : QMainWindow(nullptr)
{
    SecureMode=IsSec;
    OldDesktop=OldDesk;
    hProcess=OpenProcess(PROCESS_DUP_HANDLE|PROCESS_VM_READ|PROCESS_VM_WRITE|PROCESS_VM_OPERATION,FALSE,pid);
    if(hProcess==NULL)EXIT(GetLastError());
    char *Buffer;
    Buffer=reinterpret_cast<char*>(malloc(len));
    SIZE_T r;
    if(!ReadProcessMemory(hProcess,addr,Buffer,len,&r))ExitProcess(1);
    if(r!=len)EXIT(GetLastError());

    INT32 Byte1;
    memcpy(&Byte1,Buffer,sizeof(DWORD));
    INT32 Byte2;
    memcpy(&Byte2,Buffer+sizeof(DWORD),sizeof(DWORD));
    if(Byte1!=len)EXIT(GetLastError());

    memcpy(&OldForeign,Buffer+2*sizeof(DWORD)+2*sizeof(void*),sizeof(void*));
    memcpy(&Address,Buffer+6*sizeof(DWORD)+4*sizeof(void*),sizeof(void*));

    DWORD BASE=0;
    if(Byte2==0){//Excutable
        BASE=6*sizeof(DWORD)+6*sizeof(void*);
    }
    else if(Byte2==1){//dll
        BASE=6*sizeof(DWORD)+5*sizeof(void*);
    }
    else{//Not Understood
        INT64 t=len;
        memcpy(&t,Buffer+6*sizeof(DWORD)+8*sizeof(void*),sizeof(void*));
        if(t<len && t>0)BASE=6*sizeof(DWORD)+5*sizeof(void*);
        memcpy(&t,Buffer+6*sizeof(DWORD)+9*sizeof(void*),8);
        if(t<len && t>0)BASE=6*sizeof(DWORD)+6*sizeof(void*);
    }
    LONG DescriptionAddr;
    memcpy(&DescriptionAddr,Buffer+BASE,sizeof(void*));
    LONG FilePathAddr;
    memcpy(&FilePathAddr,Buffer+BASE+sizeof(void*),sizeof(void*));
    LONG ParamsAddr;
    memcpy(&ParamsAddr,Buffer+BASE+2*sizeof(void*),sizeof(void*));
    LONG EndAddr;
    memcpy(&ParamsAddr,Buffer+BASE+3*sizeof(void*),sizeof(void*));

    wchar_t *Description=reinterpret_cast<wchar_t*>(malloc(FilePathAddr-DescriptionAddr));
    memcpy(Description,Buffer+DescriptionAddr,FilePathAddr-DescriptionAddr);


    wchar_t *PathName=reinterpret_cast<wchar_t*>(malloc(ParamsAddr-FilePathAddr));
    memcpy(PathName,Buffer+FilePathAddr,ParamsAddr-FilePathAddr);

    this->setWindowFlag(Qt::FramelessWindowHint);
    this->setWindowFlag(Qt::Tool);
    this->setWindowFlag(Qt::WindowStaysOnTopHint);
    this->setGeometry(100,300,600,200);
    this->setAttribute(Qt::WA_TranslucentBackground);
    this->setStyleSheet("background-color: transparent;");

    QLabel *labTitle=new QLabel(this);
    labTitle->setGeometry(0,75,80,20);
    labTitle->setStyleSheet("color:rgba(255,255,255,100%);background:rgba(0,0,0,90%);");
    labTitle->setText("UAC Prompt");
    labTitle->setAlignment(Qt::AlignCenter|Qt::AlignHCenter);
    labTitle->show();

    QLabel *labText=new QLabel(this);
    labText->setGeometry(0,100,600,100);
    labText->setStyleSheet("color:rgba(255,255,255,100%);background:rgba(0,0,0,90%);");
    labText->setMargin(10);
    labText->show();
    labText->setMargin(10);
    labText->setAlignment(Qt::AlignLeft|Qt::AlignTop);

    QFontMetrics fm(labText->font());
    QString tmp;
    QString Str1=QString();
    wchar_t *Signer;
    bool Valid=VerifySign(PathName,&Signer);
    if(Valid){
        if(Byte2==1){
            tmp=QString::fromWCharArray(L"您真的要允许程序加载由 ")+QString::fromWCharArray(Signer)+QString::fromWCharArray(L" 发布的 ")+
                    QString::fromWCharArray(PathFindFileNameW(PathName))+QString::fromWCharArray(L" 吗？\n其描述为：")+
                    QString::fromWCharArray(Description);
        }else{
            tmp=QString::fromWCharArray(L"您真的要允许运行由 ")+QString::fromWCharArray(Signer)+QString::fromWCharArray(L" 发布的 ")+
                    QString::fromWCharArray(PathFindFileNameW(PathName))+QString::fromWCharArray(L" 吗？");
        }
    }
    else{
        if(Byte2==1){
            tmp=QString::fromWCharArray(L"您真的要允许加载来源不明的动态链接库 ")+QString::fromWCharArray(PathFindFileNameW(PathName))+
                    QString::fromWCharArray(L" 吗？\n其描述为：")+QString::fromWCharArray(Description);
        }else{
            tmp=QString::fromWCharArray(L"您真的要允许运行来源不明的程序 ")+QString::fromWCharArray(PathFindFileNameW(PathName))+
                    QString::fromWCharArray(L" 吗？");
        }
    }
    while(tmp.length()>0){
        int i=0;
        while(i<tmp.length() && fm.width(tmp,i)<=360)i++;
        if(i>0){
            QString temp=tmp.left(i);
            tmp=tmp.right(tmp.length()-i);
            Str1.push_back(temp);
            if(tmp.length()>0)
                Str1.push_back("\n");
        }
    }
    labText->setText(Str1);
    PushButton *pbYes=new PushButton(this);
    PushButton *pbNo=new PushButton(this);
    pbYes->setGeometry(300,170,25,20);
    pbNo->setGeometry(350,170,25,20);
    pbYes->setStyleSheet("background:rgba(0,0,0,0%);color:rgba(255,255,255,100%);font-size:20px;");
    pbNo->setStyleSheet("background:rgba(0,0,0,0%);color:rgba(255,255,255,100%);font-size:20px;");
    QLabel* labImg=new QLabel(this);
    QPixmap pixAsk=QPixmap::fromImage(QImage(":/Ask.png")).scaled(220,200);
    QPixmap pixYes=QPixmap::fromImage(QImage(":/Yes.png")).scaled(220,200);
    QPixmap pixNo=QPixmap::fromImage(QImage(":/No.png")).scaled(220,200);
    pbYes->setText(QString::fromWCharArray(L"是"));
    pbNo->setText(QString::fromWCharArray(L"否"));
    connect(pbYes,&PushButton::MouseEnter,this,[=]{
        labImg->setGeometry(380,0,pixYes.width(),pixYes.height());
        labImg->setPixmap(pixYes);
        labImg->setMask(pixYes.mask());
        labImg->show();
        labImg->repaint();
    });
    connect(pbNo,&PushButton::MouseEnter,this,[=]{
        labImg->setGeometry(380,0,pixNo.width(),pixNo.height());
        labImg->setPixmap(pixNo);
        labImg->setMask(pixNo.mask());
        labImg->show();
        labImg->repaint();
    });
    connect(pbYes,&PushButton::MouseLeave,this,[=]{
        labImg->setGeometry(380,0,pixAsk.width(),pixAsk.height());
        labImg->setPixmap(pixAsk);
        labImg->setMask(pixAsk.mask());
        labImg->show();
        labImg->repaint();
    });
    connect(pbNo,&PushButton::MouseLeave,this,[=]{
        labImg->setGeometry(380,0,pixAsk.width(),pixAsk.height());
        labImg->setPixmap(pixAsk);
        labImg->setMask(pixAsk.mask());
        labImg->show();
        labImg->repaint();
    });
    connect(pbYes,&PushButton::pressed,this,[=]{
        HANDLE NewLocal;
        HANDLE OldLocal;
        HANDLE NewForeign;

        if(0==DuplicateHandle(hProcess,OldForeign,GetCurrentProcess(),&OldLocal,NULL,FALSE,DUPLICATE_SAME_ACCESS))EXIT(GetLastError());
        DWORD len;
        if(0==GetTokenInformation(OldLocal,TokenLinkedToken,&NewLocal,sizeof(HANDLE),&len))EXIT(GetLastError());
        if(len!=sizeof(HANDLE))EXIT(1);
        if(0==DuplicateHandle(GetCurrentProcess(),NewLocal,hProcess,&NewForeign,NULL,FALSE,DUPLICATE_SAME_ACCESS))EXIT(GetLastError());
        SIZE_T written;
        if(0==WriteProcessMemory(hProcess,Address,reinterpret_cast<void*>(&NewForeign),sizeof(HANDLE),&written))EXIT(GetLastError());
        if(written!=sizeof(HANDLE))EXIT(1);
        CloseHandle(OldLocal);
        CloseHandle(NewLocal);
        CloseHandle(OldForeign);
        EXIT(0);
    });
    connect(pbNo,&PushButton::pressed,this,[=]{
        EXIT(1223);
    });
    pbYes->show();
    pbNo->show();
    pbYes->setMouseTracking(true);
    pbNo->setMouseTracking(true);

    labImg->setGeometry(380,0,pixAsk.width(),pixAsk.height());
    labImg->setPixmap(pixAsk);
    labImg->setMask(pixAsk.mask());
    labImg->show();
    labImg->repaint();

}
void MainWindow::EXIT(DWORD retCode){
    if(SecureMode){
        SwitchDesktop(OldDesktop);
        CloseDesktop(OldDesktop);
    }
    ExitProcess(retCode);
}
MainWindow::~MainWindow()
{
    EXIT(0);
}

