
# ycsb

### None:
[OVERALL], RunTime(ms), 7388
[OVERALL], Throughput(ops/sec), 676.773145641581

### Dimitris:
[OVERALL], RunTime(ms), 8763
[OVERALL], Throughput(ops/sec), 570.5808513066302

### Guozhen:
[OVERALL], RunTime(ms), 6269
[OVERALL], Throughput(ops/sec), 797.5753708725474




# function grep

### None
real    0m52.920s
user    0m10.840s
sys     0m42.043s

### Dimitris
real    0m18.177s
user    0m10.742s
sys     0m7.409s


### Guozhen
real    0m17.807s
user    0m10.698s
sys     0m7.083s



# function pwgen

### None

real    2m9.414s
user    0m16.523s
sys     1m52.846s

### Dimitris

real    0m58.360s
user    0m16.572s
sys     0m41.763s

### Guozhen

real    0m57.583s
user    0m16.560s
sys     0m40.999s


# hpcc


### None
0.000359803 Billion(10^9) Updates    per second [GUP/s]
0.000359803 Billion(10^9) Updates/PE per second [GUP/s]


### Dimitris

0.000898177 Billion(10^9) Updates    per second [GUP/s]
0.000898177 Billion(10^9) Updates/PE per second [GUP/s]


### Guozhen
0.000922192 Billion(10^9) Updates    per second [GUP/s]
0.000922192 Billion(10^9) Updates/PE per second [GUP/s]




# sysbench

### None 

Throughput:
         read:  IOPS=196596.41 48.00 MiB/s (50.33 MB/s)
         write: IOPS=0.00 0.00 MiB/s (0.00 MB/s)
         fsync: IOPS=0.00


### Dimitris

Throughput:
         read:  IOPS=396573.78 96.82 MiB/s (101.52 MB/s)
         write: IOPS=0.00 0.00 MiB/s (0.00 MB/s)
         fsync: IOPS=0.00



### Guozhen

Throughput:
         read:  IOPS=411841.86 100.55 MiB/s (105.43 MB/s)
         write: IOPS=0.00 0.00 MiB/s (0.00 MB/s)
         fsync: IOPS=0.00




# httpd

### None
Requests per second:    3974.54 [#/sec] (mean)
Time per request:       2.516 [ms] (mean)
Time per request:       0.252 [ms] (mean, across all concurrent requests)
Transfer rate:          11077.49 [Kbytes/sec] received


### Dimitris:
Requests per second:    4000.38 [#/sec] (mean)
Time per request:       2.500 [ms] (mean)
Time per request:       0.250 [ms] (mean, across all concurrent requests)
Transfer rate:          11149.50 [Kbytes/sec]


### Guozhen
Requests per second:    4361.15 [#/sec] (mean)
Time per request:       2.293 [ms] (mean)
Time per request:       0.229 [ms] (mean, across all concurrent requests)
Transfer rate:          12155.00 [Kbytes/sec] received



# nginx

### None 

Requests per second:    5273.70 [#/sec] (mean)
Time per request:       1.896 [ms] (mean)
Time per request:       0.190 [ms] (mean, across all concurrent requests)
Transfer rate:          14631.42 [Kbytes/sec] received


### Dimitris

Requests per second:    5640.29 [#/sec] (mean)
Time per request:       1.773 [ms] (mean)
Time per request:       0.177 [ms] (mean, across all concurrent requests)
Transfer rate:          15648.50 [Kbytes/sec] received

### Guozhen
Requests per second:    5854.31 [#/sec] (mean)
Time per request:       1.708 [ms] (mean)
Time per request:       0.171 [ms] (mean, across all concurrent requests)
Transfer rate:          16242.28 [Kbytes/sec]




# IPC_FIFO

### None 

Message size:       1000
Message count:      100000
Total duration:     2719.735    ms
Average duration:   27.153      us
Minimum duration:   22.272      us
Maximum duration:   323.584     us
Standard deviation: 10.615      us


### Dimitris

Message size:       1000
Message count:      100000
Total duration:     1770.708    ms
Average duration:   17.667      us
Minimum duration:   15.360      us
Maximum duration:   366.336     us
Standard deviation: 5.094       us
Message rate:       56474       msg/s





### Guozhen

Message size:       1000
Message count:      100000
Total duration:     1744.408    ms
Average duration:   17.412      us
Minimum duration:   15.104      us
Maximum duration:   262.912     us
Standard deviation: 5.180       us
Message rate:       57326       msg/s


# IPC_DOMAIN

### None

Message size:       1000
Message count:      100000
Total duration:     1798.867    ms
Average duration:   17.954      us
Minimum duration:   15.104      us
Maximum duration:   344.832     us
Standard deviation: 6.707       us
Message rate:       55590       msg/s

### Dimitris

Message size:       1000
Message count:      100000
Total duration:     1167.276    ms
Average duration:   11.638      us
Minimum duration:   9.984       us
Maximum duration:   226.304     us
Standard deviation: 4.277       us
Message rate:       85669       msg/s

### Guozhen

Message size:       1000
Message count:      100000
Total duration:     1159.390    ms
Average duration:   11.558      us
Minimum duration:   9.728       us
Maximum duration:   612.864     us
Standard deviation: 4.996       us
Message rate:       86252       msg/s


# IPC_MQ

### None

Message size:       1000
Message count:      100000
Total duration:     2259.641    ms
Average duration:   22.555      us
Minimum duration:   19.456      us
Maximum duration:   1253.888    us
Standard deviation: 9.446       us
Message rate:       44254       msg/s


### Dimitris

Message size:       1000
Message count:      100000
Total duration:     1728.768    ms
Average duration:   17.245      us
Minimum duration:   14.592      us
Maximum duration:   414.208     us
Standard deviation: 8.046       us
Message rate:       57844       msg/s


### Guozhen

Message size:       1000
Message count:      100000
Total duration:     1675.201    ms
Average duration:   16.712      us
Minimum duration:   13.568      us
Maximum duration:   1123.840    us
Standard deviation: 9.092       us
Message rate:       59694       msg/s



# Unixbench


### None
System Call Overhead                         151890.7 lps   (10.0 s, 7 samples)

System Benchmarks Partial Index              BASELINE       RESULT    INDEX
System Call Overhead                          15000.0     151890.7    101.3
                                                                   ========
System Benchmarks Index Score (Partial Only)                          101.3

------------------------------------------------------------------------
Benchmark Run: Thu Aug 08 2019 22:09:06 - 22:11:17
8 CPUs in system; running 8 parallel copies of tests

System Call Overhead                         154641.0 lps   (10.1 s, 7 samples)

System Benchmarks Partial Index              BASELINE       RESULT    INDEX
System Call Overhead                          15000.0     154641.0    103.1
                                                                   ========
System Benchmarks Index Score (Partial Only)                          103.1



### Dimitris(Crashed)

[115562.873543]  [<ffffffffbe562e41>] dump_stack+0x19/0x1b
[115562.875446]  [<ffffffffbdfbcca0>] warn_alloc_failed+0x110/0x180
[115562.875924]  [<ffffffffbe55e44e>] __alloc_pages_slowpath+0x6b6/0x724
[115562.876450]  [<ffffffffbdfb5ebe>] ? __find_get_page+0x1e/0xa0
[115562.877393]  [<ffffffffbdfc1304>] __alloc_pages_nodemask+0x404/0x420
[115562.878065]  [<ffffffffbe00e268>] alloc_pages_current+0x98/0x110
[115562.878618]  [<ffffffffbdfbba4e>] __get_free_pages+0xe/0x40
[115562.880024]  [<ffffffffbe01997e>] kmalloc_order_trace+0x2e/0xa0
[115562.880605]  [<ffffffffbe01d9c1>] __kmalloc+0x211/0x230
[115562.881202]  [<ffffffffc0aa233b>] ? create_cuckoo+0xdb/0x190 [draco_module]
[115562.881846]  [<ffffffffc0aa237c>] create_cuckoo+0x11c/0x190 [draco_module]
[115562.882385]  [<ffffffffc0aa2e07>] handle_syscall+0x2c7/0x2d0 [draco_module]
[115562.883059]  [<ffffffffc0aa2f13>] draco+0x103/0x1c0 [draco_module]
[115562.883566]  [<ffffffffbe570608>] ? __do_page_fault+0x228/0x500
[115562.884885]  [<ffffffffbdf52314>] __secure_computing+0x44/0xf0
[115562.886915]  [<ffffffffbde3aae5>] syscall_trace_enter+0x175/0x220
[115562.888109]  [<ffffffffbe576015>] tracesys+0x5d/0xc9
[115562.888619] Mem-Info:
[115562.889244] active_anon:153327 inactive_anon:95401 isolated_anon:0
 active_file:918639 inactive_file:468211 isolated_file:0
 unevictable:0 dirty:1 writeback:0 unstable:0
 slab_reclaimable:106363 slab_unreclaimable:78491
 mapped:28010 shmem:103031 pagetables:3655 bounce:0
 free:189616 free_pcp:1811 free_cma:0
[115562.892475] Node 0 DMA free:15876kB min:104kB low:128kB high:156kB active_anon:0kB inactive_anon:0kB active_file:0kB inactive_file:0kB unevictable:0kB isolated(anon):0kB isolated(file):0kB present:15996kB managed:15908kB mlocked:0kB dirty:0kB writeback:0kB mapped:0kB shmem:0kB slab_reclaimable:0kB slab_unreclaimable:32kB kernel_stack:0kB pagetables:0kB unstable:0kB bounce:0kB free_pcp:0kB local_pcp:0kB free_cma:0kB writeback_tmp:0kB pages_scanned:0 all_unreclaimable? yes
[115562.894714] lowmem_reserve[]: 0 2830 9802 9802
[115562.895314] Node 0 DMA32 free:206864kB min:19484kB low:24352kB high:29224kB active_anon:163304kB inactive_anon:100448kB active_file:1049848kB inactive_file:551504kB unevictable:0kB isolated(anon):0kB isolated(file):0kB present:3128652kB managed:2898160kB mlocked:0kB dirty:0kB writeback:0kB mapped:39524kB shmem:112248kB slab_reclaimable:166068kB slab_unreclaimable:112940kB kernel_stack:1536kB pagetables:3624kB unstable:0kB bounce:0kB free_pcp:3724kB local_pcp:656kB free_cma:0kB writeback_tmp:0kB pages_scanned:0 all_unreclaimable? no
[115562.899021] lowmem_reserve[]: 0 0 6972 6972
[115562.899701] Node 0 Normal free:535724kB min:47988kB low:59984kB high:71980kB active_anon:445140kB inactive_anon:285764kB active_file:2624708kB inactive_file:1321340kB unevictable:0kB isolated(anon):0kB isolated(file):0kB present:7340032kB managed:7140028kB mlocked:0kB dirty:4kB writeback:0kB mapped:72516kB shmem:299876kB slab_reclaimable:259384kB slab_unreclaimable:200992kB kernel_stack:7360kB pagetables:10996kB unstable:0kB bounce:0kB free_pcp:3508kB local_pcp:644kB free_cma:0kB writeback_tmp:0kB pages_scanned:1960 all_unreclaimable? no
[115562.904175] lowmem_reserve[]: 0 0 0 0
[115562.905044] Node 0 DMA: 1*4kB (U) 0*8kB 0*16kB 0*32kB 2*64kB (U) 1*128kB (U) 1*256kB (U) 0*512kB 1*1024kB (U) 1*2048kB (M) 3*4096kB (M) = 15876kB
[115562.907448] Node 0 DMA32: 1371*4kB (UEM) 1918*8kB (UEM) 2241*16kB (UEM) 1357*32kB (UEM) 1659*64kB (UEM) 5*128kB (UM) 0*256kB 0*512kB 0*1024kB 0*2048kB 0*4096kB = 206924kB
[115562.909638] Node 0 Normal: 11960*4kB (UEM) 10828*8kB (UM) 6078*16kB (UEM) 2471*32kB (UEM) 3489*64kB (UEM) 8*128kB (M) 0*256kB 0*512kB 0*1024kB 0*2048kB 0*4096kB = 535104kB
[115562.912450] Node 0 hugepages_total=0 hugepages_free=0 hugepages_surp=0 hugepages_size=1048576kB
[115562.913226] Node 0 hugepages_total=0 hugepages_free=0 hugepages_surp=0 hugepages_size=2048kB
[115562.913986] 1490025 total pagecache pages
[115562.914853] 144 pages in swap cache
[115562.915987] Swap cache stats: add 2912, delete 2768, find 1071/1236
[115562.916845] Free swap  = 5108432kB
[115562.917711] Total swap = 5111804kB
[115562.918621] Draco: Cuckoo way hashtable allocation failed
[115562.919236] BUG: unable to handle kernel 
[115562.920050] paging request at 0000000000011bb8
[115562.920764] IP: [<ffffffffc0aa2d59>] handle_syscall+0x219/0x2d0 [draco_module]
[115562.921553] PGD 0 
[115562.922297] Oops: 0000 [#1] SMP 
[115562.923616] Modules linked in: draco_module(OE) xt_nat veth ipt_MASQUERADE nf_nat_masquerade_ipv4 nf_conntrack_netlink xt_addrtype br_netfilter overlay(T) binfmt_misc ip6t_rpfilter ipt_REJECT nf_reject_ipv4 ip6t_REJECT nf_reject_ipv6 xt_conntrack ip_set nfnetlink ebtable_nat ebtable_broute bridge stp llc ip6table_nat nf_conntrack_ipv6 nf_defrag_ipv6 nf_nat_ipv6 ip6table_mangle ip6table_security ip6table_raw iptable_nat nf_conntrack_ipv4 nf_defrag_ipv4 nf_nat_ipv4 nf_nat nf_conntrack iptable_mangle iptable_security iptable_raw ebtable_filter ebtables ip6table_filter ip6_tables iptable_filter vmw_vsock_vmci_transport vsock snd_seq_midi snd_seq_midi_event vfat fat iosf_mbi ppdev crc32_pclmul ghash_clmulni_intel aesni_intel vmw_balloon lrw gf128mul glue_helper ablk_helper cryptd snd_ens1371 snd_rawmidi
[115562.928576]  snd_ac97_codec btusb btrtl btbcm ac97_bus btintel snd_seq joydev bluetooth uvcvideo snd_seq_device videobuf2_vmalloc pcspkr videobuf2_memops snd_pcm videobuf2_core videodev rfkill snd_timer snd sg soundcore vmw_vmci i2c_piix4 tpm_crb parport_pc parport ip_tables xfs libcrc32c sr_mod cdrom ata_generic sd_mod crc_t10dif crct10dif_generic pata_acpi crct10dif_pclmul crct10dif_common crc32c_intel vmwgfx drm_kms_helper syscopyarea sysfillrect serio_raw sysimgblt fb_sys_fops ttm ata_piix drm mptspi e1000 libata scsi_transport_spi mptscsih mptbase drm_panel_orientation_quirks nfit libnvdimm dm_mirror dm_region_hash dm_log dm_mod [last unloaded: draco_module]
[115562.933914] CPU: 0 PID: 101913 Comm: Run Kdump: loaded Tainted: G           OE  ------------ T 3.10.0-957.10.1.el7.draco_v1.x86_64 #1
[115562.935726] Hardware name: VMware, Inc. VMware7,1/440BX Desktop Reference Platform, BIOS VMW71.00V.9694812.B64.1808210100 08/21/2018
[115562.937558] task: ffff9b1c1a3ac100 ti: ffff9b1ccbcf8000 task.ti: ffff9b1ccbcf8000
[115562.938484] RIP: 0010:[<ffffffffc0aa2d59>]  [<ffffffffc0aa2d59>] handle_syscall+0x219/0x2d0 [draco_module]
[115562.939405] RSP: 0018:ffff9b1ccbcfbe20  EFLAGS: 00010206
[115562.940301] RAX: 0000000000002888 RBX: ffff9b1ccbcfbe88 RCX: ffffffffc0aa2d3c
[115562.941157] RDX: 0000000000000511 RSI: 0000000000000000 RDI: 0000000000011bb8
[115562.942024] RBP: ffff9b1ccbcfbe70 R08: 0000000000000000 R09: 0000000000000092
[115562.942896] R10: 00000000000fd759 R11: 000000003e848d11 R12: 0000000000000000
[115562.943785] R13: 0000000000000000 R14: ffff9b1cda0978a0 R15: ffff9b1c853e46e0
[115562.944638] FS:  00007f77003b6fc0(0000) GS:ffff9b1cf3600000(0000) knlGS:0000000000000000
[115562.945532] CS:  0010 DS: 0000 ES: 0000 CR0: 0000000080050033
[115562.946375] CR2: 0000000000011bb8 CR3: 00000001afb34000 CR4: 00000000003607f0
[115562.947208] Call Trace:
[115562.948037]  [<ffffffffc0aa2f13>] draco+0x103/0x1c0 [draco_module]
[115562.948983]  [<ffffffffbe570608>] ? __do_page_fault+0x228/0x500
[115562.949809]  [<ffffffffbdf52314>] __secure_computing+0x44/0xf0
[115562.950751]  [<ffffffffbde3aae5>] syscall_trace_enter+0x175/0x220
[115562.951588]  [<ffffffffbe576015>] tracesys+0x5d/0xc9
[115562.952458] Code: 12 41 29 c3 44 31 da 41 c1 cb 08 44 29 da 89 d0 31 d2 48 f7 75 d0 89 d7 48 8d 04 fd 00 00 00 00 48 c1 e7 06 48 29 c7 4b 03 3c c7 <80> 3f 01 74 42 41 83 c4 01 41 0f b6 c4 3b 45 c8 0f 82 71 fe ff 
[115562.954149] RIP  [<ffffffffc0aa2d59>] handle_syscall+0x219/0x2d0 [draco_module]
[115562.954962]  RSP <ffff9b1ccbcfbe20>
[115562.955700] CR2: 0000000000011bb8


### Guozhen

Benchmark Run: Thu Aug 08 2019 21:54:17 - 21:56:27
8 CPUs in system; running 1 parallel copy of tests

System Call Overhead                         305847.9 lps   (10.0 s, 7 samples)

System Benchmarks Partial Index              BASELINE       RESULT    INDEX
System Call Overhead                          15000.0     305847.9    203.9
                                                                   ========
System Benchmarks Index Score (Partial Only)                          203.9

------------------------------------------------------------------------
Benchmark Run: Thu Aug 08 2019 21:56:27 - 21:58:38
8 CPUs in system; running 8 parallel copies of tests

System Call Overhead                         306890.3 lps   (10.1 s, 7 samples)

System Benchmarks Partial Index              BASELINE       RESULT    INDEX
System Call Overhead                          15000.0     306890.3    204.6
                                                                   ========
System Benchmarks Index Score (Partial Only)                          204.6