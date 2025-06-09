package com.mycompany.app;

/**
 * Hello world!
 */
public class App {
   public static void main(String[] args) {
        // Zookeeper
        // ZKClient client = new ZKClient();
        // try {
        //     client.connect();
        //     client.runExample();
        //     client.close();
        // } catch (Exception e) {
        //     e.printStackTrace();
        // }
        KafkaClient client = new KafkaClient("localhost:9092", "demo-topic");
        client.produceMessage("Hello from refactored KafkaClient!");
        client.consumeMessages();
    }
}
