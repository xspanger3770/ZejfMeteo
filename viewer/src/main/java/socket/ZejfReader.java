package socket;

import org.tinylog.Logger;

import java.io.IOException;
import java.io.InputStream;

public class ZejfReader {

    private final InputStream inputStream;

    public ZejfReader(InputStream in){
        inputStream = in;
    }

    public void run(){
        new Thread(){
            @Override
            public void run() {
                runReader();
            }
        }.start();
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
                        System.out.println(line.toString()); // TODO
                        line.setLength(0);
                    } else {
                        line.append(ch);
                    }
                }
            }
        } catch (IOException e) {
            e.printStackTrace();
            Logger.error(e);
        }

        onClose();
    }

    public void onClose() {

    }

}
