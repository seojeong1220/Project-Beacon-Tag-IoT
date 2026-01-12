<!DOCTYPE html>
<html lang="ko">
<head>
<meta charset="UTF-8">
<meta http-equiv="refresh" content="3">
<title>WALLET TAG Monitor</title>

<style>
body {
    font-family: Arial, sans-serif;
}


.card {
    width: 90%;
    max-width: 700px;
    margin: 20px auto;
    background: white;
    border: 1px solid #ddd;
    border-radius: 12px;
    padding: 20px;
    text-align: center;
}
.card h1 { margin: 6px 0; font-size: 28px; }
.card h2 { margin: 6px 0; font-size: 20px; }
.card h3 { margin: 6px 0; font-size: 18px; }


.human-map {
    width: 520px;
    height: 520px;
    margin: 40px auto 30px auto;
    position: relative;
}


.circle {
    position: absolute;
    border: 2px dashed #ccc;
    border-radius: 50%;
}
.c10 { width:520px; height:520px; top:0; left:0; }
.c5  { width:380px; height:380px; top:70px; left:70px; }
.c3  { width:280px; height:280px; top:120px; left:120px; }
.c1  { width:160px; height:160px; top:180px; left:180px; }



.person {
    position: absolute;
    font-size: 88px; 
    left: 50%;
    top: 50%;
    transform: translate(-50%, -50%);
}

.wallet {
    position: absolute;
    font-size: 44px;
    transition: left 0.6s ease, top 0.6s ease;
}

.map-label {
    text-align: center;
    font-size: 13px;
    color: #666;
    margin-bottom: 40px;
}
.person.found {
    animation: pulse 1.2s infinite;
}

@keyframes pulse {
    0% {
        transform: translate(-50%, -50%) scale(1);
    }
    50% {
        transform: translate(-50%, -50%) scale(1.12);
    }
    100% {
        transform: translate(-50%, -50%) scale(1);
    }
}

</style>
</head>

<?php
$conn = mysqli_connect("localhost", "iot", "pwiot", "iotdb");
if (!$conn) {
    die("DB 연결 실패");
}


$q = "
    SELECT ts, rssi, distance_m
    FROM beacon_log
    ORDER BY ts DESC
    LIMIT 1
";
$r = mysqli_query($conn, $q);
$row = mysqli_fetch_assoc($r);
mysqli_close($conn);

$latestDist = $row ? (float)$row['distance_m'] : 0;
$latestRssi = $row ? (int)$row['rssi'] : 0;
$latestTs   = $row ? $row['ts'] : "-";


if ($latestDist >= 10.0) {
    $status = "VERY FAR";
    $statusColor = "#e74c3c";
} elseif ($latestDist >= 5.0) {
    $status = "FAR";
    $statusColor = "#f39c12";
} elseif ($latestDist >= 1.0) {
    $status = "NEAR";
    $statusColor = "#27ae60";
} else {
    $status = "VERY NEAR";
    $statusColor = "#2ecc71";
}

$isFound = ($latestDist < 0.5);


$maxDist = 10.0;              
$ratio   = min($latestDist / $maxDist, 1.0);
$radius = $ratio * 260;   
    

$angle = deg2rad(45);         

if ($isFound) {
    $x = 50;
    $y = 50;
} else {
    $x = 50 + ($radius / 420 * 100) * cos($angle);
    $y = 50 + ($radius / 420 * 100) * sin($angle);
}

?>

<body>


<div class="card">
    <h1>WALLET TAG</h1>
    <h2>
        Distance: <?= number_format($latestDist, 1) ?> m
        &nbsp; | &nbsp;
        RSSI: <?= $latestRssi ?>
    </h2>
    <h3 style="color:<?= $statusColor ?>; font-weight:bold;">
        Status: <?= $status ?>
    </h3>
    <div style="font-size:13px; color:#777;">
        Last update: <?= $latestTs ?>
    </div>
</div>


<div class="human-map">

    
    <div class="circle c10"></div>
    <div class="circle c5"></div>
    <div class="circle c3"></div>
    <div class="circle c1"></div>

    
    <div class="person <?= $isFound ? 'found' : '' ?>">
    <?= $isFound ? '🙋' : '🧍' ?>
</div>

    <div class="wallet" style="left:<?= $x ?>%; top:<?= $y ?>%;">👛</div>
</div>


</body>
</html>
