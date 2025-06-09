package com.mycompany.app;

import org.apache.zookeeper.*;

import java.io.IOException;
import java.util.concurrent.CountDownLatch;

public class ZKClient {

    private ZooKeeper zooKeeper;
    private static final String CONNECT_STRING = "localhost:2181";
    private static final int SESSION_TIMEOUT = 3000;

    public void connect() throws IOException, InterruptedException {
        CountDownLatch connectedSignal = new CountDownLatch(1);

        zooKeeper = new ZooKeeper(CONNECT_STRING, SESSION_TIMEOUT, event -> {
            if (event.getState() == Watcher.Event.KeeperState.SyncConnected) {
                connectedSignal.countDown();
                System.out.println("✅ Connected to ZooKeeper");
            }
        });

        connectedSignal.await();
    }

    public void runExample() throws KeeperException, InterruptedException {
        String path = "/myapp";

        if (zooKeeper.exists(path, false) == null) {
            zooKeeper.create(path, "hello".getBytes(), ZooDefs.Ids.OPEN_ACL_UNSAFE, CreateMode.PERSISTENT);
            System.out.println("Created znode " + path);
        }

        byte[] data = zooKeeper.getData(path, false, null);
        System.out.println("Read znode data: " + new String(data));

        zooKeeper.getData(path, watchedEvent -> {
            if (watchedEvent.getType() == Watcher.Event.EventType.NodeDataChanged) {
                try {
                    byte[] newData = zooKeeper.getData(path, false, null);
                    System.out.println("Znode data changed: " + new String(newData));
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }
        }, null);

        zooKeeper.setData(path, "updated".getBytes(), -1);
        Thread.sleep(3000);
    }

    public void close() throws InterruptedException {
        zooKeeper.close();
    }
}
