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
        displayedVariables.add(new DisplayedVariable(11, "Temperature", "˚C"));
        displayedVariables.add(new DisplayedVariable(12, "Humidity", "%"));
        displayedVariables.add(new DisplayedVariable(13, "Dewpoint", "˚C"));
        displayedVariables.add(new DisplayedVariable(1, "Pressure", "hPa"));
        displayedVariables.add(new DisplayedVariable(8, "Wspd", "km/h"));
        displayedVariables.add(new DisplayedVariable(9, "Gust", "km/h"));
        displayedVariables.add(new DisplayedVariable(10, "Wdir", "˚"));
        displayedVariables.add(new DisplayedVariable(15, "Solar panel voltage", "V"));
        displayedVariables.add(new DisplayedVariable(16, "Battery voltage", "V"));
        displayedVariables.add(new DisplayedVariable(17, "Battery current", "mA"));
    }

    @Override
    public void paint(Graphics gr) {
        super.paint(gr);
        Graphics2D g = (Graphics2D) gr;
        g.setColor(Color.black);
        g.setFont(new Font("Calibri", Font.BOLD, 14));

        int y = 20;

        for(DisplayedVariable displayedVariable:displayedVariables){
            g.drawString(String.format("%s: %.2f%s", displayedVariable.name, displayedVariable.lastValue, displayedVariable.units), 2, y);
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

    class DisplayedVariable{
        int variableId;
        double lastValue = DataManager.VAL_ERROR;
        String name;
        String units;

        public DisplayedVariable(int variableId, String name, String units) {
            this.variableId = variableId;
            this.name = name;
            this.units = units;
        }
    }
}
