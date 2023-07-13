package data;

import exception.FatalApplicationException;
import exception.FatalIOException;
import exception.RuntimeApplicationException;
import main.ZejfMeteo;
import time.TimeUtils;

import java.io.*;
import java.sql.Time;
import java.util.*;

public class DataManager {

    public static final int PERMANENTLY_LOADED_HOURS = 24;
    public static final double VAL_ERROR = -999.0;
    public static final double VAL_NOT_MEASURED = -998.0;
    private final List<DataHour> dataHours;

    public static final File DATA_FOLDER = new File(ZejfMeteo.MAIN_FOLDER, "/data/");

    public DataManager(){
        dataHours = new LinkedList<>();

        load();

        Timer timer = new Timer();
        TimerTask task = new TimerTask() {
            @Override
            public void run() {
                saveAll();
            }
        };

        // Schedule the task to run every two minutes, starting from now
        timer.schedule(task, 30*1000, 2 * 60 * 1000);
    }

    public static final String[] MONTHS = new String[] { "January", "February", "March", "April", "May", "June", "July",
            "August", "September", "October", "November", "December" };

    public void load(){
        Calendar calendar = Calendar.getInstance();
        calendar.setTime(new Date());

        System.out.printf("Current hour number = %d\n", TimeUtils.getHourNumber(calendar));

        for(int i = 0; i < PERMANENTLY_LOADED_HOURS; i++){
            try {
                dataHourGet(calendar, true, true);
            } catch (FatalApplicationException | RuntimeApplicationException e) {
                ZejfMeteo.handleException(e);
            }

            calendar.add(Calendar.HOUR, -1);
        }
    }

    public DataHour dataHourFind(long hourNumber){
        for(DataHour dataHour:dataHours){
            if(dataHour.getHourNumber() == hourNumber){
                return dataHour;
            }
        }
        return null;
    }

    public DataHour dataHourGet(Calendar calendar, boolean load, boolean createNew) throws FatalApplicationException{
        DataHour result = dataHourFind(TimeUtils.getHourNumber(calendar));

        boolean add = false;

        if(result == null && load){
            result = loadHour(calendar);
            add = true;
        }

        if(result == null && createNew){
            result = new DataHour(TimeUtils.getHourNumber(calendar));
        }

        if(result != null && add){
            dataHours.add(result);
        }

        return result;
    }

    public static File getDataHourFile(Calendar calendar){
        String str = String.format("/%d/%s/%02d/%02dH_%d.dat", calendar.get(Calendar.YEAR), MONTHS[calendar.get(Calendar.MONTH)],
                calendar.get(Calendar.DATE), calendar.get(Calendar.HOUR_OF_DAY), TimeUtils.getHourNumber(calendar));
        return new File(DATA_FOLDER, str);
    }

    private DataHour loadHour(Calendar calendar) throws FatalApplicationException {
        File file = getDataHourFile(calendar);
        System.out.println("Load: "+file.getAbsoluteFile());
        if(!file.exists()){
            return null;
        }

        try{
            ObjectInputStream in = new ObjectInputStream(new FileInputStream(file));
            return (DataHour) in.readObject();
        } catch(ClassNotFoundException | InvalidClassException e) {
            //throw new FatalApplicationException("Unable to load", e);
            File brokenFile = new File(file.getAbsolutePath() + "_err_"+System.currentTimeMillis());
            if(!file.renameTo(brokenFile)){
                throw new FatalApplicationException(new IOException("Unable to rename broken file "+brokenFile.getAbsolutePath()));
            }
            throw new RuntimeApplicationException(String.format("Could not load %s, renamed to %s", file.getName(), brokenFile.getName()));
        } catch (IOException e) {
            throw new FatalIOException(e);
        }
    }

    private void saveHour(DataHour dataHour) throws FatalApplicationException {
        if(dataHour == null || !dataHour.isModified()){
            return;
        }
        Calendar calendar = Calendar.getInstance();
        calendar.setTimeInMillis(dataHour.getHourNumber() * 1000 * 60 * 60L);
        File file = getDataHourFile(calendar);
        if (!file.getParentFile().exists()) {
            if(!file.getParentFile().mkdirs()){
                throw new FatalApplicationException(new IOException("Unable to create directories for data"));
            }
        }
        System.out.println("Save: "+file.getAbsoluteFile());

        try {
            ObjectOutputStream out = new ObjectOutputStream(new FileOutputStream(file));
            out.writeObject(dataHour);
        } catch (IOException e) {
            throw new FatalApplicationException("Unable to save", e);
        }
    }

    public void saveAll(){
        for(DataHour dataHour : dataHours){
            try {
                saveHour(dataHour);
            } catch (FatalApplicationException e) {
                ZejfMeteo.handleException(e);
            }
        }
    }

}
