unit ufrmMain;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, Forms, Controls, Graphics, Dialogs, Menus, ExtCtrls;

type

  { TfrmMain }

  TfrmMain = class(TForm)
    mnuAbrir: TMenuItem;
    Separator1: TMenuItem;
    mnuSalir: TMenuItem;
    mnuMain: TPopupMenu;
    tryMain: TTrayIcon;
    procedure FormCreate(Sender: TObject);
    procedure mnuAbrirClick(Sender: TObject);
    procedure mnuSalirClick(Sender: TObject);
  private
    deviceAddress: String;
    devicePort: Integer;
    doorKey: String;
  public

  end;

var
  frmMain: TfrmMain;

implementation

{$R *.lfm}

{ TfrmMain }

uses IniFiles, Sockets;

procedure TfrmMain.mnuSalirClick(Sender: TObject);
begin
  Close;
end;


procedure TfrmMain.mnuAbrirClick(Sender: TObject);
var
  Sock: Longint;
  Addr: TInetSockAddr;
  Msg: AnsiString;
begin
  // Mostramos el mensaje
  tryMain.ShowBalloonHint;
  Application.ProcessMessages;

  // Configuramos la dirección y el puerto
  Addr.sin_family:= AF_INET;
  Addr.sin_port:= htons(devicePort);
  Addr.sin_addr:= StrToNetAddr(deviceAddress);

  // El mensaje a enviar
  Msg:= 'pulse:' + doorKey;

  // Creamos un socket UDP
  Sock:= fpSocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if Sock < 0 then
    raise Exception.Create('Failed to create socket');

  // Enviamos el mensaje
  if fpSendTo(Sock, @Msg[1], Length(Msg), 0, @Addr, SizeOf(Addr)) < 0 then
    raise Exception.Create('Failed to send message');

  // Cerramos el socket
  CloseSocket(Sock);
end;

procedure TfrmMain.FormCreate(Sender: TObject);
begin
  // Leemos el fichero de configuracion
  with TIniFile.Create(ChangeFileExt(ParamStr(0), '.ini')) do
  try
    deviceAddress:= ReadString('device', 'address', EmptyStr);
    devicePort:= ReadInteger('device','port', 61978);

    doorKey:= ReadString('door', 'key', EmptyStr);
  finally
    Free;
  end
end;

end.

