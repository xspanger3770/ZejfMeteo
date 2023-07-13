package ui;

import data.DataManager;

import javax.swing.*;
import java.awt.*;
import java.util.ArrayList;
import java.util.List;

public class SimpleRealtimePanel extends JPanel {

    private final List<DisplayedVariable> displayedVariables;

    public SimpleRealtimePanel(){
        displayedVariables = new ArrayList<>();
        displayedVariables.add(new DisplayedVariable(11, "Temperature", "˚C", 3));
        displayedVariables.add(new DisplayedVariable(12, "Humidity", "%", 2));
        displayedVariables.add(new DisplayedVariable(13, "Dewpoint", "˚C", 2));
        displayedVariables.add(new DisplayedVariable(1, "Pressure", "hPa", 3));
        displayedVariables.add(new DisplayedVariable(8, "Wspd 1 min", "km/h", 2));
        displayedVariables.add(new DisplayedVariable(9, "Gust now", "km/h", 2));
        displayedVariables.add(new DisplayedVariable(10, "Wdir 1 min", "˚", 2));
        displayedVariables.add(new DisplayedVariable(3, "Wspd 10 min", "km/h",3));
        displayedVariables.add(new DisplayedVariable(4, "Gust 1 min", "km/h", 2));
        displayedVariables.add(new DisplayedVariable(5, "Wdir 10 min", "˚", 2));
        displayedVariables.add(new DisplayedVariable(15, "Solar panel voltage", "V", 3));
        displayedVariables.add(new DisplayedVariable(16, "Battery voltage", "V", 3));
        displayedVariables.add(new DisplayedVariable(17, "Battery current", "mA", 2));

        displayedVariables.add(new DisplayedVariable(7, "Temperature puda", "˚C", 2));
        displayedVariables.add(new DisplayedVariable(2, "Temperature sklep", "˚C", 3));
    }

    @Override
    public void paint(Graphics gr) {
        super.paint(gr);
        Graphics2D g = (Graphics2D) gr;
        g.setColor(Color.black);
        g.setFont(new Font("Calibri", Font.BOLD, 14));

        int y = 20;

        for(DisplayedVariable displayedVariable:displayedVariables){
            g.drawString(String.format("%s: %."+displayedVariable.decimals+"f%s", displayedVariable.name, displayedVariable.lastValue, displayedVariable.units), 2, y);
            y += 22;
        }
    }

    public void log(int variableId, double value) {
        for(DisplayedVariable displayedVariable:displayedVariables){
            if(displayedVariable.variableId == variableId){
                displayedVariable.lastValue = value;
            }
        }
        repaint();
    }

    static class DisplayedVariable{
        private final int decimals;
        int variableId;
        double lastValue = DataManager.VAL_ERROR;
        String name;
        String units;

        public DisplayedVariable(int variableId, String name, String units, int decimals) {
            this.variableId = variableId;
            this.name = name;
            this.units = units;
            this.decimals = decimals;
        }
    }
}
