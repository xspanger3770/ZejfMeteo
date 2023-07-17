package main;

import data.DataManager;
import exception.ApplicationErrorHandler;
import exception.FatalApplicationException;
import exception.FatalIOException;
import exception.RuntimeApplicationException;
import org.tinylog.Logger;
import ui.ZejfFrame;

import javax.swing.*;
import java.awt.event.WindowAdapter;
import java.awt.event.WindowEvent;
import java.io.File;
import java.io.IOException;

public class ZejfMeteo {

    public static final String VERSION = "0.0.1";
    public static final File MAIN_FOLDER = new File("./ZejfMeteoViewer");
    private static ZejfFrame frame;
    private static ApplicationErrorHandler errorHandler;
    private static DataManager dataManager;

    public static void main(String[] args) throws Exception {
        if(!MAIN_FOLDER.exists()){
            if(!MAIN_FOLDER.mkdirs()){
                throw new FatalIOException(new IOException("Failed to create main folder"));
            }
        }

        errorHandler = new ApplicationErrorHandler(frame);
        Thread.setDefaultUncaughtExceptionHandler(errorHandler);

        dataManager = new DataManager();

        Settings.loadProperties();
        SwingUtilities.invokeLater(() -> {
            frame = new ZejfFrame();

            frame.addWindowListener(new WindowAdapter() {
                @Override
                public void windowClosing(WindowEvent e) {
                    try {
                        destroy();
                    } catch (FatalApplicationException ex) {
                        ZejfMeteo.handleException(ex);
                    }
                }
            });

            frame.setVisible(true);
            frame.setStatus("Ready");
        });
    }

    private static void destroy() throws FatalApplicationException {
        dataManager.saveAll();
    }

    public static void handleException(Exception e) {
        if (!(e instanceof RuntimeApplicationException)) {
            Logger.error("Caught exception : {}", e.getMessage());
            Logger.error(e);
        }

        if(getFrame() != null){
            getFrame().setStatus(String.format("Error: %s", e.getMessage()));
        }

        errorHandler.handleException(e);
    }

    public static DataManager getDataManager() {
        return dataManager;
    }

    public static ZejfFrame getFrame() {
        return frame;
    }
}
