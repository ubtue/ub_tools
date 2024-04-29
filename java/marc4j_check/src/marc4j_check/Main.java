/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package marc4j_check;

import java.io.InputStream;
import java.io.FileInputStream;
import org.marc4j.*;
import org.marc4j.marc.Record;

/**
 *
 * @author root
 */
public class Main {
    private static void usage() {
        System.err.println("Usage: marc4j_check.jar <filename>");
        System.exit(1);
    }

    /**
     * @param args the command line arguments
     */
    public static void main(String[] args) {
        if (args.length != 1) {
            usage();
        }
        String file = args[0];
        System.err.println("checking file: " + file);

        try {
            InputStream input = new FileInputStream(file);
            MarcReader reader = new MarcStreamReader(input);
            while (reader.hasNext()) {
                Record record = (Record) reader.next();
                System.out.println("found valid record: " + record.getControlNumber());
            }
            System.exit(0);
        } catch (java.io.FileNotFoundException e) {
            System.err.println(e.getLocalizedMessage());
        } catch (Exception e) {
            System.err.println("caught exception: " + e.getLocalizedMessage());
            e.printStackTrace();
        }

        System.exit(1);
    }
}
