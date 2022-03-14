/*
 Navicat Premium Data Transfer

 Source Server         : 123
 Source Server Type    : MySQL
 Source Server Version : 80019
 Source Host           : localhost:3306
 Source Schema         : property

 Target Server Type    : MySQL
 Target Server Version : 80019
 File Encoding         : 65001

 Date: 21/03/2020 21:48:43
*/

SET NAMES utf8mb4;
SET FOREIGN_KEY_CHECKS = 0;

-- ----------------------------
-- Table structure for charge
-- ----------------------------
DROP TABLE IF EXISTS `charge`;
CREATE TABLE `charge`  (
  `chargeID` int(0) NOT NULL,
  `chargePay` int(0) DEFAULT NULL,
  PRIMARY KEY (`chargeID`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = ascii COLLATE = ascii_general_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Records of charge
-- ----------------------------
INSERT INTO `charge` VALUES (1, 200);
INSERT INTO `charge` VALUES (2, 300);
INSERT INTO `charge` VALUES (3, 150);
INSERT INTO `charge` VALUES (4, 250);

-- ----------------------------
-- Table structure for house
-- ----------------------------
DROP TABLE IF EXISTS `house`;
CREATE TABLE `house`  (
  `houseID` int(0) NOT NULL,
  `houseYear` int(0) DEFAULT NULL,
  `houseArea` float DEFAULT NULL,
  PRIMARY KEY (`houseID`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = ascii COLLATE = ascii_general_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Records of house
-- ----------------------------
INSERT INTO `house` VALUES (101, 2004, 1000);
INSERT INTO `house` VALUES (102, 2006, 1200);
INSERT INTO `house` VALUES (103, 2011, 700);
INSERT INTO `house` VALUES (104, 2012, 800);
INSERT INTO `house` VALUES (105, 2012, 900);

-- ----------------------------
-- Table structure for ower
-- ----------------------------
DROP TABLE IF EXISTS `ower`;
CREATE TABLE `ower`  (
  `owerID` int(0) NOT NULL,
  `owerName` char(12) CHARACTER SET ascii COLLATE ascii_general_ci DEFAULT NULL,
  `owerSex` char(1) CHARACTER SET ascii COLLATE ascii_general_ci DEFAULT NULL,
  `owerAge` int(0) DEFAULT NULL,
  `houseID` int(0) DEFAULT NULL,
  `roomID` int(0) NOT NULL,
  PRIMARY KEY (`owerID`) USING BTREE,
  INDEX `houseID`(`houseID`) USING BTREE,
  INDEX `ower_ibfk_2`(`roomID`) USING BTREE,
  CONSTRAINT `ower_ibfk_1` FOREIGN KEY (`houseID`) REFERENCES `house` (`houseID`) ON DELETE RESTRICT ON UPDATE RESTRICT,
  CONSTRAINT `ower_ibfk_2` FOREIGN KEY (`roomID`) REFERENCES `room` (`roomID`) ON DELETE RESTRICT ON UPDATE RESTRICT
) ENGINE = InnoDB CHARACTER SET = ascii COLLATE = ascii_general_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Records of ower
-- ----------------------------
INSERT INTO `ower` VALUES (1, 'Wang Yi', 'M', 30, 101, 1010101);
INSERT INTO `ower` VALUES (2, 'Sun Er', 'M', 25, 101, 1010207);
INSERT INTO `ower` VALUES (3, 'Zhang San', 'M', 26, 102, 1020711);
INSERT INTO `ower` VALUES (4, 'Li Si', 'F', 32, 102, 1020603);
INSERT INTO `ower` VALUES (5, 'Zhao Wu', 'M', 29, 103, 1031102);
INSERT INTO `ower` VALUES (6, 'Qian Liu', 'F', 41, 103, 1031203);
INSERT INTO `ower` VALUES (7, 'Zhou Qi', 'M', 32, 104, 1040102);
INSERT INTO `ower` VALUES (8, 'Wu Ba', 'F', 22, 104, 1040507);
INSERT INTO `ower` VALUES (9, 'Zheng Jiu', 'M', 31, 105, 1050702);
INSERT INTO `ower` VALUES (10, 'Han Shi', 'F', 27, 105, 1050809);

-- ----------------------------
-- Table structure for ower_family
-- ----------------------------
DROP TABLE IF EXISTS `ower_family`;
CREATE TABLE `ower_family`  (
  `ofID` int(0) NOT NULL,
  `ofName` char(20) CHARACTER SET ascii COLLATE ascii_general_ci DEFAULT NULL,
  `ofSex` char(1) CHARACTER SET ascii COLLATE ascii_general_ci DEFAULT NULL,
  `ofAge` int(0) DEFAULT NULL,
  `owerID` int(0) DEFAULT NULL,
  PRIMARY KEY (`ofID`) USING BTREE,
  INDEX `owerID`(`owerID`) USING BTREE,
  INDEX `family`(`ofID`, `owerID`) USING BTREE,
  CONSTRAINT `ower_family_ibfk_1` FOREIGN KEY (`owerID`) REFERENCES `ower` (`owerID`) ON DELETE RESTRICT ON UPDATE RESTRICT
) ENGINE = InnoDB CHARACTER SET = ascii COLLATE = ascii_general_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Records of ower_family
-- ----------------------------
INSERT INTO `ower_family` VALUES (101, 'Wang Xiaoyi', 'M', 12, 1);
INSERT INTO `ower_family` VALUES (201, 'Sun XiaoEr', 'F', 13, 2);
INSERT INTO `ower_family` VALUES (301, 'Zhang Xiaosan', 'M', 8, 3);
INSERT INTO `ower_family` VALUES (401, 'Li Xiaosi', 'F', 6, 4);
INSERT INTO `ower_family` VALUES (501, 'Zhao Xiaowu', 'M', 11, 5);
INSERT INTO `ower_family` VALUES (601, 'Qian Xiaoliu', 'F', 10, 6);
INSERT INTO `ower_family` VALUES (701, 'Zhou Xiaoqi', 'M', 9, 7);
INSERT INTO `ower_family` VALUES (801, 'Wu Xiaoba', 'F', 8, 8);
INSERT INTO `ower_family` VALUES (901, 'Zheng Xiaojiu', 'F', 14, 9);
INSERT INTO `ower_family` VALUES (1001, 'Han Xiaoshi', 'M', 15, 10);

-- ----------------------------
-- Table structure for pay
-- ----------------------------
DROP TABLE IF EXISTS `pay`;
CREATE TABLE `pay`  (
  `payID` int(0) NOT NULL,
  `payTime` date DEFAULT NULL,
  `payReal` float DEFAULT NULL,
  `chargeID` int(0) DEFAULT NULL,
  `owerID` int(0) DEFAULT NULL,
  PRIMARY KEY (`payID`) USING BTREE,
  INDEX `owerID`(`owerID`) USING BTREE,
  INDEX `payment`(`chargeID`, `owerID`) USING BTREE,
  CONSTRAINT `pay_ibfk_1` FOREIGN KEY (`chargeID`) REFERENCES `charge` (`chargeID`) ON DELETE RESTRICT ON UPDATE RESTRICT,
  CONSTRAINT `pay_ibfk_2` FOREIGN KEY (`owerID`) REFERENCES `ower` (`owerID`) ON DELETE RESTRICT ON UPDATE RESTRICT
) ENGINE = InnoDB CHARACTER SET = ascii COLLATE = ascii_general_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Records of pay
-- ----------------------------
INSERT INTO `pay` VALUES (1, '2020-09-01', 200, 1, 1);
INSERT INTO `pay` VALUES (2, '2020-09-02', 300, 2, 2);
INSERT INTO `pay` VALUES (3, '2020-09-03', 150, 3, 3);
INSERT INTO `pay` VALUES (4, '2020-09-03', 250, 4, 4);
INSERT INTO `pay` VALUES (5, '2020-09-04', 200, 1, 5);
INSERT INTO `pay` VALUES (6, '2020-09-07', 150, 3, 6);
INSERT INTO `pay` VALUES (7, '2020-09-11', 300, 2, 7);
INSERT INTO `pay` VALUES (8, '2020-09-14', 200, 1, 8);
INSERT INTO `pay` VALUES (9, '2020-09-17', 250, 4, 9);
INSERT INTO `pay` VALUES (10, '2020-09-22', 300, 2, 10);

-- ----------------------------
-- Table structure for room
-- ----------------------------
DROP TABLE IF EXISTS `room`;
CREATE TABLE `room`  (
  `roomID` int(0) NOT NULL,
  `roomType` char(24) CHARACTER SET ascii COLLATE ascii_general_ci DEFAULT NULL,
  `roomArea` float DEFAULT NULL,
  `roomTime` date DEFAULT NULL,
  `houseID` int(0) DEFAULT NULL,
  `owerID` int(0) DEFAULT NULL,
  PRIMARY KEY (`roomID`) USING BTREE,
  INDEX `houseID`(`houseID`) USING BTREE,
  INDEX `roomstruct`(`roomType`, `roomArea`) USING BTREE,
  CONSTRAINT `room_ibfk_1` FOREIGN KEY (`houseID`) REFERENCES `house` (`houseID`) ON DELETE RESTRICT ON UPDATE RESTRICT
) ENGINE = InnoDB CHARACTER SET = ascii COLLATE = ascii_general_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Records of room
-- ----------------------------
INSERT INTO `room` VALUES (1010101, 'BIG', 220, '2010-09-09', 101, 1);
INSERT INTO `room` VALUES (1010207, 'SMALL', 75, '2010-10-10', 101, 2);
INSERT INTO `room` VALUES (1020603, 'SMALL', 70, '2012-07-02', 102, 4);
INSERT INTO `room` VALUES (1020711, 'MEDIUM', 120, '2012-08-02', 102, 3);
INSERT INTO `room` VALUES (1031102, 'SMALL', 85, '2014-08-03', 103, 5);
INSERT INTO `room` VALUES (1031203, 'BIG', 200, '2014-04-21', 103, 6);
INSERT INTO `room` VALUES (1040102, 'MEDIUM', 120, '2016-09-23', 104, 7);
INSERT INTO `room` VALUES (1040507, 'MEDIUM', 110, '2016-11-12', 104, 8);
INSERT INTO `room` VALUES (1050702, 'BIG', 200, '2018-01-13', 105, 9);
INSERT INTO `room` VALUES (1050809, 'SMALL', 80, '2018-12-15', 105, 10);

SET FOREIGN_KEY_CHECKS = 1;
