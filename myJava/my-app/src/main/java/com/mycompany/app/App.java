package com.mycompany.app;

/**
 * Hello world!
 */
public class App {
   public static void main(String[] args) {
        ZKClient client = new ZKClient();
        try {
            client.connect();
            client.runExample();
            client.close();
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}
