-- Schema for rfid_access database
-- Auto-executed on first MySQL container startup

CREATE DATABASE IF NOT EXISTS rfid_access;
USE rfid_access;

CREATE TABLE IF NOT EXISTS users (
    id          INT AUTO_INCREMENT PRIMARY KEY,
    uid         VARCHAR(50)  NOT NULL UNIQUE,
    name        VARCHAR(100) NOT NULL,
    created_at  TIMESTAMP    DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS logs (
    id            INT AUTO_INCREMENT PRIMARY KEY,
    uid           VARCHAR(50) NOT NULL,
    access_status ENUM('GRANTED', 'DENIED') NOT NULL,
    timestamp     TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
