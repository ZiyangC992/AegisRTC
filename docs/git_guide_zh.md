# Git 操作手册（AegisRTC）

## 一、Git 的四个位置

```text
工作区 --git add--> 暂存区 --git commit--> 本地仓库 --git push--> GitHub
```

- 工作区：正在编辑的文件。
- 暂存区：下一次提交准备包含的文件。
- 本地仓库：已经提交的历史。
- 远程仓库：GitHub 上的项目历史。

## 二、分支

`main` 是稳定版本；`feature/linux-v4l2` 是 Linux 开发版本。
`origin/main` 是本地记录的 GitHub `main`，不是另一个工作目录。

```bash
git branch                 # 查看本地分支
git branch -a              # 查看所有分支
git switch main            # 切换分支
git switch -c feature/test # 创建并切换新分支
```

## 三、创建本地仓库并上传

```bash
cd D:\MyProject
git init
git config --global user.name "Your Name"
git config --global user.email "you@example.com"
git remote add origin https://github.com/user/repo.git
git add 文件1 文件2
git commit -m "Initial commit"
git branch -M main
git push -u origin main
```

参数说明：

- `init`：创建 `.git` 仓库目录。
- `config --global`：设置当前用户的全局信息。
- `remote add origin URL`：添加远程仓库，`origin` 是远程别名。
- `add`：把文件放入暂存区。
- `commit -m`：创建本地提交，`-m` 后面是提交说明。
- `branch -M`：重命名当前分支。
- `push -u origin main`：上传并建立跟踪关系。

## 四、查看修改

```bash
git status              # 查看状态
git status --short      # 简短状态
git diff                # 查看未暂存修改
git diff --cached       # 查看已暂存修改
git log --oneline --graph --all # 查看提交历史
git remote -v           # 查看远程地址
```

状态中的 `M` 表示已修改，`??` 表示未跟踪的新文件。

## 五、开发分支上传

```bash
git switch -c feature/linux-v4l2
git add 文件1 文件2 文件3
git commit -m "Implement Linux V4L2 capture"
git push -u origin feature/linux-v4l2
```

`switch -c` 创建并切换分支；首次 `push -u` 会让本地分支跟踪远程分支。

## 六、下载 GitHub 项目和分支

第一次下载：

```bash
git clone https://github.com/user/repo.git
cd repo
```

已有项目同步：

```bash
git fetch origin
git pull --rebase origin main
```

`fetch` 只下载远程信息；`pull` 下载并整合远程提交；`--rebase` 让本地提交排在远程最新提交之后。

下载开发分支：

```bash
git fetch origin
git switch --track origin/feature/linux-v4l2
git pull --rebase origin feature/linux-v4l2
```

`--track` 会创建本地分支并跟踪远程分支。

## 七、Windows/Linux 同步

Linux 上传：

```bash
git switch feature/linux-v4l2
git add network/udp_transport.cpp
git commit -m "Update Linux UDP transport"
git push origin feature/linux-v4l2
```

Windows 下载 Linux 分支：

```powershell
git fetch origin
git switch feature/linux-v4l2
git pull --rebase origin feature/linux-v4l2
```

此时得到的是 Linux 分支，不是合并后的 `main`。Pull Request 合并后，下载完整版本：

```powershell
git switch main
git pull --rebase origin main
```

Linux 专属代码应在 Ubuntu 测试，Windows 专属代码应在 Windows 测试。

## 八、未提交修改

如果出现 `You have unstaged changes`：

```bash
git stash push -u -m "Save local changes before sync"
git pull --rebase origin main
git stash pop
```

`stash` 临时保存修改；`-u` 同时保存未跟踪文件；`pop` 恢复保存内容。

## 九、冲突处理

冲突文件中会出现：

```text
<<<<<<< HEAD
远程版本
=======
本地版本
>>>>>>> commit-id
```

删除标记并保留最终内容，然后：

```bash
git add 冲突文件
git rebase --continue
```

放弃本次变基：

```bash
git rebase --abort
```

## 十、常见错误

- `non-fast-forward`：远程比本地新，先执行 `git pull --rebase`。
- `Recv failure: Connection was reset`：GitHub 网络连接中断，可执行 `git ls-remote origin` 测试。
- `Not Run`：测试程序没有成功生成，先重新编译。

## 十一、日常模板

```bash
git switch 当前分支
git pull --rebase origin 当前分支
git status
git diff
git add 需要提交的文件
git diff --cached
git commit -m "Describe the change"
git push origin 当前分支
```

