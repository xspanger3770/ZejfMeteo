package main;

import exception.FatalIOException;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.Properties;

public class Settings {

	private static final File propertiesFile = new File(Main.MAIN_FOLDER, "properties.properties");
	public static String ADDRESS;
	public static int PORT;
	static void loadProperties() throws FatalIOException {
		Properties prop = new Properties();
		try {
			if (!propertiesFile.exists()) {
				if(!propertiesFile.createNewFile()){
					throw new FatalIOException(new IOException("Failed to create properties file"));
				}
			}
			prop.load(new FileInputStream(propertiesFile));
		} catch (IOException e) {
			throw new FatalIOException("Failed to load properties", e);
		}
		ADDRESS = String.valueOf(prop.getProperty("address", "0.0.0.0"));
		PORT = Integer.parseInt(prop.getProperty("port", "1955"));
		saveProperties();
	}

	public static void saveProperties() throws FatalIOException {
		Properties prop = new Properties();
		prop.setProperty("address", ADDRESS);
		prop.setProperty("port", String.valueOf(PORT));
		try {
			prop.store(new FileOutputStream(propertiesFile), "");
		} catch (IOException e) {
			throw new FatalIOException("Failed to store properties", e);
		}
	}

}
