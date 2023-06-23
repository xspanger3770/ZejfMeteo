package main;

import exception.FatalIOException;
import ui.ZejfFrame;

import javax.swing.*;
import java.io.File;
import java.io.IOException;

public class Main {

    public static final String VERSION = "0.0.1";
    public static final File MAIN_FOLDER = new File("./ZejfMeteoViewer");

    public static void main(String[] args) throws Exception {
        if(!MAIN_FOLDER.exists()){
            if(!MAIN_FOLDER.mkdirs()){
                throw new FatalIOException(new IOException("Failed to create main folder"));
            }
        }
        Settings.loadProperties();
        SwingUtilities.invokeLater(() -> new ZejfFrame().setVisible(true));
    }

}
