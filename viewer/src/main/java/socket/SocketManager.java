package socket;

import exception.RuntimeApplicationException;
import main.ZejfMeteo;
import main.Settings;
import org.tinylog.Logger;

import java.io.IOException;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;

public class SocketManager {

    public static final int CONNECT_TIMEOUT = 5000;
    public static final int SO_TIMEOUT = 10000;

    private Thread socketThread;
    private volatile boolean socketRunning = false;
    private Socket socket;
    private ZejfCommunicator reader;

    public SocketManager(){
        ScheduledExecutorService executor = Executors.newScheduledThreadPool(1);
        Runnable task = () -> {
            if(socket != null && socket.isConnected()){
                try {
                    socket.getOutputStream().write("heartbeat\n".getBytes(StandardCharsets.UTF_8));
                } catch (IOException e) {
                    Logger.error(e);
                }
            }
        };

        executor.scheduleAtFixedRate(task, 0, 5, TimeUnit.SECONDS);
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
                    ZejfMeteo.handleException(e);
                }
            }
        };
        socketThread.start();
    }

    private void runSocket(String ip, int port) {
        ZejfMeteo.getFrame().setStatus("Connecting...");
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
        reader = new ZejfCommunicator(socket.getInputStream(), socket.getOutputStream()){
            @Override
            public void onClose() {
                socketRunning = false;
                ZejfMeteo.getFrame().setStatus("Disconnected");
            }

            @Override
            public void onReceive(Packet packet) {
                System.out.printf("Packet command %d from %d to %d: %s\n", packet.command(), packet.from(), packet.to(), packet.message());
            }
        };

        reader.run();
        ZejfMeteo.getFrame().setStatus(String.format("Connected to %s:%d", socket.getInetAddress().getHostAddress(), socket.getPort()));
    }

    public boolean isSocketRunning() {
        return socketRunning;
    }

    public void close() {
        if(socket != null){
            try {
                socket.close();
            }catch(IOException e){
                throw new RuntimeApplicationException("Cannot close socket", e);
            }
        }
    }
}
