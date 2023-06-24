package socket;

import exception.FatalIOException;
import exception.RuntimeApplicationException;
import main.Main;
import main.Settings;

import java.io.IOException;
import java.net.InetSocketAddress;
import java.net.Socket;

public class SocketManager {

    public static final int CONNECT_TIMEOUT = 5000;
    public static final int SO_TIMEOUT = 10000;

    private Thread socketThread;
    private volatile boolean socketRunning = false;
    private Socket socket;
    private ZejfReader reader;

    public SocketManager(){

    }

    public void connect() {
        System.out.println(socketRunning);
        if(socketRunning){
            throw new RuntimeApplicationException("Socket already running!");
        }
        socketRunning = true;
        socketThread = new Thread("Socket"){
            @Override
            public void run() {
                try {
                    runSocket(Settings.ADDRESS, Settings.PORT);
                }catch(RuntimeApplicationException e){
                    Main.handleException(e);
                }
            }
        };
        socketThread.start();
    }

    private void runSocket(String ip, int port) {
        System.out.println("Connect "+ ip+":"+port);
        socket = new Socket();
        try {
            socket.connect(new InetSocketAddress(ip, port), CONNECT_TIMEOUT);
            socket.setSoTimeout(SO_TIMEOUT);
            runReader();
        } catch(IOException e) {
            socketRunning = false;
            throw new RuntimeApplicationException(String.format("Cannot connect to %s:%s", ip, port), e);
        }
    }

    private void runReader() throws IOException{
        reader = new ZejfReader(socket.getInputStream()){
            @Override
            public void onClose() {
                socketRunning = false;
            }
        };

        reader.run();
    }

    public boolean isSocketRunning() {
        return socketRunning;
    }
}
