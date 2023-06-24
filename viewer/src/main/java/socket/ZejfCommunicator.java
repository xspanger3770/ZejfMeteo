package socket;

import org.tinylog.Logger;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;

public class ZejfCommunicator {

    public static final int COMMAND_RIP = 0x01;
    public static final int COMMAND_ID_REQUEST = 0x0f;
    public static final int COMMAND_ID_INFO = 0x10;
    public static final int COMMAND_DATA_SUBSCRIBE = 0x11;
    private final InputStream inputStream;
    private final OutputStream outputStream;

    private int deviceId = -1;

    private int serverDeviceId = -1;
    private ScheduledExecutorService executor;

    public ZejfCommunicator(InputStream in, OutputStream out){
        inputStream = in;
        outputStream = out;
    }

    public void run(){
        deviceId = -1;
        new Thread(this::runReader).start();

        executor = Executors.newScheduledThreadPool(1);
        Runnable task = () -> {
            try {
                if(deviceId == -1){
                    Packet pack = create_packet(0, COMMAND_ID_REQUEST, "");
                    sendPacket(pack);
                } else{
                    sendRIP();
                    sendPacket(create_packet(serverDeviceId, COMMAND_DATA_SUBSCRIBE, ""));
                }
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
        };

        executor.scheduleAtFixedRate(task, 0, 5, TimeUnit.SECONDS);
    }

    private void sendRIP() throws IOException{
        Packet pack = create_packet(serverDeviceId, COMMAND_RIP, "");
        sendPacket(pack);
    }

    private Packet create_packet(int to, int command, String msg) {
        return new Packet(deviceId, to, 0, 0, command, 0, msg.length(), msg);
    }

    private void runReader() {
        byte[] buffer = new byte[2048];
        int count;
        try {
            StringBuilder line = new StringBuilder();
            while ((count = inputStream.read(buffer, 0, 2048)) > 0) {
                for (int i = 0; i < count; i++) {
                    char ch = (char) buffer[i];
                    if (ch == '\n') {
                        parseLine(line.toString());
                        line.setLength(0);
                    } else {
                        line.append(ch);
                    }
                }
            }
        } catch (IOException e) {
            Logger.error(e);
        }

        executor.shutdown();

        onClose();
    }

    boolean processPacket(Packet packet){
        switch (packet.command()){
            case COMMAND_ID_INFO -> {
                deviceId = packet.to();
                serverDeviceId = packet.from();
                System.out.println("Our device id is #"+deviceId);
                try {
                    sendRIP();
                } catch (IOException e) {
                    throw new RuntimeException(e);
                }
                return true;
            }

            default -> {
                return false;
            }
        }
    }

    private void parseLine(String string) {
        Packet packet = Packet.fromString(string);
        if(packet != null && !processPacket(packet)){
            onReceive(packet);
        }
    }

    public void sendPacket(Packet packet) throws IOException {
        outputStream.write(packet.toString().getBytes(StandardCharsets.UTF_8));
    }

    public void onReceive(Packet packet) {
        System.out.println("packet received");
    }

    public void onClose() {

    }

}
