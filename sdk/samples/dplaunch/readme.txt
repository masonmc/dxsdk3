DPLAUNCH
--------

Sample application to demonstrate how to launch a DirectPlay application
using the IDirectPlayLobby::RunApplication function.

Instructions:

1) Choose which application to launch. This dropdown listbox uses
   IDirectPlayLobby::EnumLocalApplications to see what applications
   are available.
2) Choose which service provider to use.
3) Choose the player name for this machine
4) Choose either "Host Session" and supply a name for the session
        - or -
   Choose "Join Session" and supply the network address of the machine
   that is hosting the session.  IPX requires no network address,
   Internet TCP/IP requires an IP address or machine name, modem requires
   a phone number to call.
5) Click on "Run Application"

Notes:

- Make sure you use DPLAUNCH to launch an application on one machine
  with the "Host Session" option first.  Then launch the application
  on other machines (using DPLAUNCH) with the same service provider using
  the "Join Session" option and the network address of the host (if needed)

- DPLAUNCH will only join sessions that were created using DPLAUNCH. The
  reason is that the session instance GUID is hard coded in DPLAUNCH.
  Normally, DirectPlay generates one (if the application was not lobbied)
  or a lobby server generates one and passes it to the lobby clients.

- A real lobby client would be an application similar to DPLAUNCH but it
  would be communicating with some lobby server to obtain all the
  parameters for the IDirectPlayLobby::RunApplication call.

- DirectPlay 3 does not include any APIs specifically geared towards
  implementing a lobby server.

