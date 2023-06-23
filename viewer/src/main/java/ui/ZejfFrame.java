package ui;

import main.Main;

import javax.swing.*;
import java.awt.*;

public class ZejfFrame extends JFrame {

    public ZejfFrame(){
        setTitle("ZejfMeteoViewer "+ Main.VERSION);
        getContentPane().setPreferredSize(new Dimension(1000, 700));
        setResizable(true);
        setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);

        getContentPane().add(createTabbedPane());
        setJMenuBar(createMenuBar());

        pack();
        setLocationRelativeTo(null);
    }

    private JMenuBar createMenuBar() {
        JMenuBar jMenuBar = new JMenuBar();

        JMenu menuConnection = new JMenu("Connection");
        menuConnection.add(new JMenuItem("Socket"));

        JMenu menuOptions = new JMenu("Options");
        menuOptions.add(new JMenuItem("Settings"));

        jMenuBar.add(menuConnection);
        jMenuBar.add(menuOptions);

        return jMenuBar;
    }

    JTabbedPane createTabbedPane(){
        JTabbedPane tabbedPane = new JTabbedPane();

        tabbedPane.addTab("Realtime", new JPanel());
        tabbedPane.addTab("Graphs", new JPanel());
        tabbedPane.addTab("Statistics", new JPanel());

        return tabbedPane;
    }

}
